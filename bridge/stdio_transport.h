#ifndef GODOT_MCP_BRIDGE_STDIO_TRANSPORT_H
#define GODOT_MCP_BRIDGE_STDIO_TRANSPORT_H

#include <string>

/// Reads JSON-RPC messages from stdin and writes responses to stdout.
/// All diagnostic logging goes to stderr to avoid corrupting the MCP protocol stream.
class StdioTransport {
public:
    /// Constructor sets stdin/stdout to binary mode on Windows
    /// to prevent \r\n translation that would corrupt JSON messages.
    StdioTransport();

    /// Reads one newline-delimited message from stdin.
    /// Returns false on EOF or read failure.
    bool read_message(std::string& out_message);

    /// Writes a message followed by a newline to stdout, then flushes.
    /// CRITICAL: single newline, no pretty printing.
    void write_message(const std::string& message);

    /// Writes a diagnostic log message to stderr.
    /// Safe to call at any time -- does not interfere with MCP protocol.
    void log(const std::string& msg);
};

#endif // GODOT_MCP_BRIDGE_STDIO_TRANSPORT_H
