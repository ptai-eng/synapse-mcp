#include <synapse/server.hpp>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#ifndef _WIN32
#include <dirent.h>
#endif

using namespace std::chrono_literals;

namespace {
std::size_t resources() {
#ifdef _WIN32
    DWORD count = 0;
    if (!GetProcessHandleCount(GetCurrentProcess(), &count)) throw std::runtime_error("handle count");
    return count;
#else
    auto* directory = opendir("/proc/self/fd");
    if (!directory) throw std::runtime_error("fd count");
    std::size_t count = 0;
    while (auto* entry = readdir(directory))
        if (entry->d_name[0] != '.') ++count;
    closedir(directory);
    return count;
#endif
}
void cycle() {
    synapse::stdio_transport transport;
    synapse::server server(transport);
    if (!server.start()) throw std::runtime_error("start");
    server.stop();
    auto done = server.wait_for(1500ms);
    if (!done || !*done) throw std::runtime_error("cycle did not join");
}
void await_file(const std::filesystem::path& path, const std::atomic<bool>* unwind = nullptr) {
    while (!(unwind && unwind->load()) && !std::filesystem::exists(path))
        std::this_thread::sleep_for(2ms);
}
struct release_on_scope_exit {
    std::atomic<bool>& release;
    ~release_on_scope_exit() { release.store(true); }
};
}

int main(int argc, char** argv) try {
    const std::string mode = argc > 1 ? argv[1] : "idle";
    if (mode == "cleanup") {
        cycle(); // Warm thread/runtime and native I/O allocation before baseline.
        const auto before = resources();
        for (int i = 0; i != 32; ++i) cycle();
        const auto after = resources();
        std::cerr << "COUNTS " << before << ' ' << after << '\n';
        return after == before ? 0 : 2;
    }
    // Every callback capture outlives the server's destructor and joined work.
    const auto gate = argc > 2 ? std::filesystem::path(argv[2]) : std::filesystem::path{};
    std::atomic<bool> unwind_release{false};
    synapse::stdio_transport transport;
    synapse::server server(transport);
    // On any early return/exception, release the test gate before server joins.
    // This is independent of filesystem success and does not release normal waits.
    release_on_scope_exit release{unwind_release};
    if (!server.register_tool({"work", "Controlled process work", {{"type", "object"}}},
        [&server, &mode, &gate, &unwind_release](const nlohmann::json&, const synapse::request_context& context)
            -> std::expected<synapse::tool_result, synapse::handler_error> {
            std::cerr << "ENTERED\n";
            if (mode == "cooperative") {
                while (!context.stop.stop_requested()) std::this_thread::sleep_for(1ms);
                std::cerr << "CANCELLED\n";
            } else if (mode == "gated") {
                await_file(gate / "release", &unwind_release);
                std::cerr << "RELEASED\n";
            } else if (mode == "stop") {
                await_file(gate / "stop", &unwind_release);
                server.stop();
            }
            return synapse::tool_result{"completed"};
        })) return 3;
    if (!server.start()) return 4;
    std::cerr << "READY\n";
    if (mode == "blocked") {
        // A multi-megabyte admitted queue exceeds the child's stdout pipe and
        // Node's unread stream buffer. Parent never consumes stdout in this case.
        const nlohmann::json frame{{"payload", std::string(900000, 'x')}};
        for (int i = 0; i != 4; ++i) if (!transport.send(frame)) return 5;
        std::cerr << "QUEUED\n";
        await_file(gate / "stop");
        server.stop();
    } else if (mode == "gated") {
        await_file(gate / "stop");
        server.stop();
        const auto done = server.wait_for(100ms);
        if (!done || *done) { std::cerr << "WAIT_INVALID\n"; return 6; }
        std::cerr << "WAIT_FALSE\n";
    }
    if (!server.wait()) return 7;
    std::cerr << "JOINED\n";
    return 0;
} catch (...) { std::cerr << "FIXTURE_FAILED\n"; return 8; }
