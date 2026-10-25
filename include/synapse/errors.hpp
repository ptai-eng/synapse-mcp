#pragma once

#include <string>

namespace synapse {

enum class sdk_errc {
    invalid_state,
    invalid_schema,
    io_error,
    unsupported_stdio,
    resource_exhausted
};

struct sdk_error {
    sdk_errc code;
    std::string message;
};

struct handler_error {
    std::string public_message;
};

} // namespace synapse
