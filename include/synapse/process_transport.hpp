#pragma once

#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <iostream>
#include <nlohmann/json.hpp>
#include "protocol.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#endif

namespace synapse {

class process_transport {
public:
    using message_callback = std::function<void(const nlohmann::json&)>;

    process_transport(const std::string& command, const std::vector<std::string>& args) {
#ifdef _WIN32
        SECURITY_ATTRIBUTES saAttr;
        saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
        saAttr.bInheritHandle = TRUE;
        saAttr.lpSecurityDescriptor = NULL;

        if (!CreatePipe(&hChildStd_OUT_Rd, &hChildStd_OUT_Wr, &saAttr, 0)) {
            throw std::runtime_error("StdoutRd CreatePipe failed");
        }
        if (!SetHandleInformation(hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0)) {
            throw std::runtime_error("Stdout SetHandleInformation failed");
        }

        if (!CreatePipe(&hChildStd_IN_Rd, &hChildStd_IN_Wr, &saAttr, 0)) {
            throw std::runtime_error("Stdin CreatePipe failed");
        }
        if (!SetHandleInformation(hChildStd_IN_Wr, HANDLE_FLAG_INHERIT, 0)) {
            throw std::runtime_error("Stdin SetHandleInformation failed");
        }

        std::string cmdline = command;
        for (const auto& arg : args) {
            cmdline += " " + arg;
        }

        PROCESS_INFORMATION piProcInfo;
        ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

        STARTUPINFOA siStartInfo;
        ZeroMemory(&siStartInfo, sizeof(STARTUPINFOA));
        siStartInfo.cb = sizeof(STARTUPINFOA);
        // We only pipe standard output and standard input.
        // We let standard error go to the parent's standard error.
        siStartInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
        siStartInfo.hStdOutput = hChildStd_OUT_Wr;
        siStartInfo.hStdInput = hChildStd_IN_Rd;
        siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

        BOOL bSuccess = CreateProcessA(NULL,
            (LPSTR)cmdline.c_str(),
            NULL,
            NULL,
            TRUE,
            0,
            NULL,
            NULL,
            &siStartInfo,
            &piProcInfo);

        if (!bSuccess) {
            throw std::runtime_error("CreateProcess failed");
        }
        
        CloseHandle(piProcInfo.hProcess);
        CloseHandle(piProcInfo.hThread);
        CloseHandle(hChildStd_OUT_Wr);
        CloseHandle(hChildStd_IN_Rd);
#else
        int pipe_in[2];
        int pipe_out[2];
        if (pipe(pipe_in) == -1 || pipe(pipe_out) == -1) {
            throw std::runtime_error("Pipe failed");
        }
        pid_ = fork();
        if (pid_ == -1) {
            throw std::runtime_error("Fork failed");
        } else if (pid_ == 0) {
            // Child
            close(pipe_in[1]);
            close(pipe_out[0]);
            dup2(pipe_in[0], STDIN_FILENO);
            dup2(pipe_out[1], STDOUT_FILENO);
            close(pipe_in[0]);
            close(pipe_out[1]);

            std::vector<char*> c_args;
            c_args.push_back(const_cast<char*>(command.c_str()));
            for (const auto& arg : args) {
                c_args.push_back(const_cast<char*>(arg.c_str()));
            }
            c_args.push_back(nullptr);
            execvp(command.c_str(), c_args.data());
            exit(1);
        } else {
            // Parent
            close(pipe_in[0]);
            close(pipe_out[1]);
            stdin_fd_ = pipe_in[1];
            stdout_fd_ = pipe_out[0];
        }
#endif
    }

    ~process_transport() {
        stop();
    }

    void start(message_callback on_message) {
        if (running_) return;
        running_ = true;
        callback_ = std::move(on_message);

        reader_thread_ = std::jthread([this](std::stop_token stoken) {
            std::string line;
            while (!stoken.stop_requested()) {
                char c;
                bool read_success = false;
#ifdef _WIN32
                DWORD dwRead;
                read_success = ReadFile(hChildStd_OUT_Rd, &c, 1, &dwRead, NULL) && (dwRead == 1);
#else
                read_success = (read(stdout_fd_, &c, 1) == 1);
#endif
                if (read_success) {
                    if (c == '\n') {
                        if (!line.empty()) {
                            try {
                                auto j = nlohmann::json::parse(line);
                                if (callback_) callback_(j);
                            } catch (...) {
                            }
                            line.clear();
                        }
                    } else if (c != '\r') {
                        line += c;
                    }
                } else {
                    break;
                }
            }
            running_ = false;
        });
    }

    void stop() {
        if (running_) {
            reader_thread_.request_stop();
#ifdef _WIN32
            if (hChildStd_OUT_Rd) { CloseHandle(hChildStd_OUT_Rd); hChildStd_OUT_Rd = NULL; }
            if (hChildStd_IN_Wr) { CloseHandle(hChildStd_IN_Wr); hChildStd_IN_Wr = NULL; }
#else
            if (stdout_fd_ != -1) { close(stdout_fd_); stdout_fd_ = -1; }
            if (stdin_fd_ != -1) { close(stdin_fd_); stdin_fd_ = -1; }
            if (pid_ > 0) {
                kill(pid_, SIGTERM);
                waitpid(pid_, nullptr, 0);
                pid_ = -1;
            }
#endif
        }
        running_ = false;
    }

    void send(const nlohmann::json& message) {
        std::lock_guard<std::mutex> lock(write_mutex_);
        std::string payload = message.dump() + "\n";
#ifdef _WIN32
        DWORD dwWritten;
        WriteFile(hChildStd_IN_Wr, payload.c_str(), (DWORD)payload.length(), &dwWritten, NULL);
#else
        write(stdin_fd_, payload.c_str(), payload.length());
#endif
    }

    void wait() {
        if (reader_thread_.joinable()) {
            reader_thread_.join();
        }
    }

private:
#ifdef _WIN32
    HANDLE hChildStd_IN_Wr = NULL;
    HANDLE hChildStd_IN_Rd = NULL;
    HANDLE hChildStd_OUT_Wr = NULL;
    HANDLE hChildStd_OUT_Rd = NULL;
#else
    int stdin_fd_ = -1;
    int stdout_fd_ = -1;
    pid_t pid_ = -1;
#endif
    std::jthread reader_thread_;
    std::mutex write_mutex_;
    message_callback callback_;
    std::atomic<bool> running_{false};
};

} // namespace synapse
