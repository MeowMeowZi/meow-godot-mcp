#include "stdio_transport.h"

#include <iostream>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

StdioTransport::StdioTransport() {
#ifdef _WIN32
    // Set stdin and stdout to binary mode on Windows.
    // This prevents the C runtime from translating \n to \r\n on output
    // and \r\n to \n on input, which would corrupt JSON-RPC messages.
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
}

bool StdioTransport::read_message(std::string& out_message) {
    std::string line;
    if (!std::getline(std::cin, line)) {
        return false; // EOF or read failure
    }

    // Trim trailing \r if present (Windows safety -- in case binary mode
    // is not fully effective or input comes from a source that uses \r\n).
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }

    out_message = std::move(line);
    return true;
}

void StdioTransport::write_message(const std::string& message) {
    std::cout << message << "\n" << std::flush;
}

void StdioTransport::log(const std::string& msg) {
    std::cerr << "[godot-mcp-bridge] " << msg << std::endl;
}
