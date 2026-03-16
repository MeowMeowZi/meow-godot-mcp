#include "tcp_client.h"

#include <cstring>
#include <iostream>
#include <thread>
#include <chrono>

#ifdef _WIN32
// Link hint for MSVC (SCons also adds ws2_32 via LIBS)
#pragma comment(lib, "ws2_32.lib")

static constexpr SOCKET INVALID_SOCK = INVALID_SOCKET;

static void close_socket(SOCKET s) {
    closesocket(s);
}

#else
static constexpr int INVALID_SOCK = -1;

static void close_socket(int s) {
    close(s);
}
#endif

// --------------------------------------------------------------------------
// Winsock initialization (Windows only, called once)
// --------------------------------------------------------------------------

void TcpClient::ensure_winsock_initialized() {
#ifdef _WIN32
    static bool initialized = false;
    if (!initialized) {
        WSADATA wsa_data;
        int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
        if (result != 0) {
            std::cerr << "[godot-mcp-bridge] WSAStartup failed with error: "
                      << result << std::endl;
            return;
        }
        initialized = true;

        // Register cleanup at process exit
        std::atexit([]() {
            WSACleanup();
        });
    }
#endif
}

// --------------------------------------------------------------------------
// Constructor / Destructor
// --------------------------------------------------------------------------

TcpClient::TcpClient(const std::string& host, int port)
    : sock_(INVALID_SOCK), host_(host), port_(port), connected_(false) {
    ensure_winsock_initialized();
}

TcpClient::~TcpClient() {
    disconnect();
}

// --------------------------------------------------------------------------
// connect_with_retry
// --------------------------------------------------------------------------

bool TcpClient::connect_with_retry(int max_retries, int retry_interval_ms) {
    for (int attempt = 1; attempt <= max_retries; ++attempt) {
        // Create TCP socket
#ifdef _WIN32
        sock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock_ == INVALID_SOCKET) {
#else
        sock_ = socket(AF_INET, SOCK_STREAM, 0);
        if (sock_ < 0) {
#endif
            std::cerr << "[godot-mcp-bridge] Failed to create socket" << std::endl;
            return false;
        }

        // Resolve address
        struct sockaddr_in server_addr;
        std::memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(static_cast<unsigned short>(port_));

        if (inet_pton(AF_INET, host_.c_str(), &server_addr.sin_addr) <= 0) {
            std::cerr << "[godot-mcp-bridge] Invalid address: " << host_ << std::endl;
            close_socket(sock_);
            sock_ = INVALID_SOCK;
            return false;
        }

        // Attempt connection
        int result = ::connect(sock_,
                               reinterpret_cast<struct sockaddr*>(&server_addr),
                               sizeof(server_addr));

        if (result == 0) {
            connected_ = true;
            read_buffer_.clear();
            std::cerr << "[godot-mcp-bridge] Connected to " << host_
                      << ":" << port_ << " on attempt " << attempt << std::endl;
            return true;
        }

        // Connection failed
        close_socket(sock_);
        sock_ = INVALID_SOCK;

        if (attempt < max_retries) {
            std::cerr << "[godot-mcp-bridge] Attempt " << attempt << "/" << max_retries
                      << ": connection refused, retrying in "
                      << (retry_interval_ms / 1000) << "s..." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(retry_interval_ms));
        }
    }

    std::cerr << "[godot-mcp-bridge] Failed to connect after " << max_retries
              << " attempts (" << (max_retries * retry_interval_ms / 1000)
              << "s timeout). Ensure Godot is running with MCP Meow plugin enabled."
              << std::endl;
    return false;
}

// --------------------------------------------------------------------------
// send_message
// --------------------------------------------------------------------------

bool TcpClient::send_message(const std::string& message) {
    if (!connected_) {
        return false;
    }

    std::string data = message + "\n";
    const char* buf = data.c_str();
    size_t remaining = data.size();

    while (remaining > 0) {
#ifdef _WIN32
        int sent = send(sock_, buf, static_cast<int>(remaining), 0);
#else
        ssize_t sent = send(sock_, buf, remaining, 0);
#endif
        if (sent <= 0) {
            std::cerr << "[godot-mcp-bridge] Send error, disconnecting" << std::endl;
            connected_ = false;
            return false;
        }
        buf += sent;
        remaining -= static_cast<size_t>(sent);
    }

    return true;
}

// --------------------------------------------------------------------------
// receive_message
// --------------------------------------------------------------------------

bool TcpClient::receive_message(std::string& out_message) {
    if (!connected_) {
        return false;
    }

    // First, check if we already have a complete line in the buffer
    size_t newline_pos = read_buffer_.find('\n');
    if (newline_pos != std::string::npos) {
        out_message = read_buffer_.substr(0, newline_pos);
        read_buffer_.erase(0, newline_pos + 1);

        // Trim trailing \r if present
        if (!out_message.empty() && out_message.back() == '\r') {
            out_message.pop_back();
        }
        return true;
    }

    // Use select() with 100ms timeout to check if data is available
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(sock_, &read_fds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000; // 100ms

#ifdef _WIN32
    // On Windows, the first argument to select() is ignored
    int ready = select(0, &read_fds, nullptr, nullptr, &timeout);
#else
    int ready = select(sock_ + 1, &read_fds, nullptr, nullptr, &timeout);
#endif

    if (ready < 0) {
        std::cerr << "[godot-mcp-bridge] select() error, disconnecting" << std::endl;
        connected_ = false;
        return false;
    }

    if (ready == 0) {
        // Timeout -- no data available, not an error
        return false;
    }

    // Data is available, read it
    char buf[4096];
#ifdef _WIN32
    int bytes_read = recv(sock_, buf, sizeof(buf), 0);
#else
    ssize_t bytes_read = recv(sock_, buf, sizeof(buf), 0);
#endif

    if (bytes_read <= 0) {
        if (bytes_read == 0) {
            std::cerr << "[godot-mcp-bridge] Server disconnected" << std::endl;
        } else {
            std::cerr << "[godot-mcp-bridge] recv() error, disconnecting" << std::endl;
        }
        connected_ = false;
        return false;
    }

    read_buffer_.append(buf, static_cast<size_t>(bytes_read));

    // Check for complete line
    newline_pos = read_buffer_.find('\n');
    if (newline_pos != std::string::npos) {
        out_message = read_buffer_.substr(0, newline_pos);
        read_buffer_.erase(0, newline_pos + 1);

        // Trim trailing \r if present
        if (!out_message.empty() && out_message.back() == '\r') {
            out_message.pop_back();
        }
        return true;
    }

    // Data received but no complete line yet
    return false;
}

// --------------------------------------------------------------------------
// disconnect
// --------------------------------------------------------------------------

void TcpClient::disconnect() {
    if (sock_ != INVALID_SOCK) {
        close_socket(sock_);
        sock_ = INVALID_SOCK;
    }
    connected_ = false;
    read_buffer_.clear();
}

// --------------------------------------------------------------------------
// is_connected
// --------------------------------------------------------------------------

bool TcpClient::is_connected() const {
    return connected_;
}
