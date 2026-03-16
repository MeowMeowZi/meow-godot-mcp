#ifndef GODOT_MCP_BRIDGE_TCP_CLIENT_H
#define GODOT_MCP_BRIDGE_TCP_CLIENT_H

#include <string>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#endif

/// TCP client that connects to the GDExtension's TCP server.
/// Sends and receives newline-delimited JSON messages.
class TcpClient {
public:
    /// Construct a TCP client targeting the given host and port.
    TcpClient(const std::string& host, int port);

    /// Destructor -- disconnects if still connected.
    ~TcpClient();

    // Non-copyable
    TcpClient(const TcpClient&) = delete;
    TcpClient& operator=(const TcpClient&) = delete;

    /// Attempt to connect with fixed-interval retry.
    /// Retries every retry_interval_ms milliseconds, up to max_retries attempts.
    /// Default: 15 attempts * 2000ms = 30 second timeout.
    /// Logs each attempt to stderr.
    /// Returns true on successful connection, false if all retries exhausted.
    bool connect_with_retry(int max_retries = 15, int retry_interval_ms = 2000);

    /// Send a message over TCP with newline framing.
    /// Appends "\n" to the message and sends the full buffer.
    /// Returns false on send error (sets connected to false).
    bool send_message(const std::string& message);

    /// Try to receive a complete newline-delimited message from TCP.
    /// Uses select() with 100ms timeout for non-blocking check.
    /// On success, sets out_message to the received line and returns true.
    /// Returns false on disconnect (recv returns 0) or error.
    /// Returns false with no error when timeout expires with no complete message.
    bool receive_message(std::string& out_message);

    /// Disconnect from the server and close the socket.
    void disconnect();

    /// Check if currently connected.
    bool is_connected() const;

private:
#ifdef _WIN32
    SOCKET sock_;
#else
    int sock_;
#endif
    std::string host_;
    int port_;
    bool connected_;
    std::string read_buffer_; ///< Accumulates TCP data for newline framing

    /// Initialize Winsock on Windows (called once).
    static void ensure_winsock_initialized();
};

#endif // GODOT_MCP_BRIDGE_TCP_CLIENT_H
