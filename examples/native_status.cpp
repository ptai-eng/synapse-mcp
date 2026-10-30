#include "native_status_state.hpp"
#include <iostream>

int main() {
    native_status_state state;
    synapse::stdio_transport transport;
    synapse::server server(transport, {"native-status", "0.3.0"});
    if (!register_native_status(server, state)) { std::cerr << "Cannot register application methods\n"; return 1; }
    if (!server.start()) { std::cerr << "Cannot start server\n"; return 1; }
    if (!server.wait()) { std::cerr << "Cannot join server\n"; return 1; }
    return 0;
}
