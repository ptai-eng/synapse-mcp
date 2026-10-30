#pragma once

#ifdef _WIN32
#include <synapse/detail/stdio_windows.hpp>
namespace synapse::detail {
using native_stdio_endpoint = HANDLE;
using native_stdio = windows_stdio;
inline native_stdio_endpoint standard_input() noexcept { return ::GetStdHandle(STD_INPUT_HANDLE); }
inline native_stdio_endpoint standard_output() noexcept { return ::GetStdHandle(STD_OUTPUT_HANDLE); }
} // namespace synapse::detail
#else
#include <synapse/detail/stdio_posix.hpp>
namespace synapse::detail {
using native_stdio_endpoint = int;
using native_stdio = posix_stdio;
inline native_stdio_endpoint standard_input() noexcept { return STDIN_FILENO; }
inline native_stdio_endpoint standard_output() noexcept { return STDOUT_FILENO; }
} // namespace synapse::detail
#endif
