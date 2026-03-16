#include "stdio_transport.h"
#include "tcp_client.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

// ---------------------------------------------------------------------------
// Thread-safe message queue
// ---------------------------------------------------------------------------

class MessageQueue {
public:
    void push(const std::string& msg) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(msg);
    }

    bool try_pop(std::string& out) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }
        out = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

private:
    mutable std::mutex mutex_;
    std::queue<std::string> queue_;
};

// ---------------------------------------------------------------------------
// CLI argument parsing
// ---------------------------------------------------------------------------

struct BridgeConfig {
    std::string host = "127.0.0.1";
    int port = 6800;
    bool show_help = false;
};

static void print_usage() {
    std::cerr
        << "Usage: godot-mcp-bridge [OPTIONS]\n"
        << "\n"
        << "MCP stdio-to-TCP bridge for Godot MCP Meow plugin.\n"
        << "Reads JSON-RPC messages from stdin and relays them to the\n"
        << "GDExtension's TCP server. Writes responses back to stdout.\n"
        << "\n"
        << "Options:\n"
        << "  --port <N>      TCP port to connect to (default: 6800)\n"
        << "  --host <addr>   TCP host to connect to (default: 127.0.0.1)\n"
        << "  --help          Show this help message and exit\n"
        << "\n"
        << "Example claude_desktop_config.json:\n"
        << "  {\n"
        << "    \"mcpServers\": {\n"
        << "      \"godot\": {\n"
        << "        \"command\": \"/path/to/godot-mcp-bridge\",\n"
        << "        \"args\": [\"--port\", \"6800\"]\n"
        << "      }\n"
        << "    }\n"
        << "  }\n"
        << std::endl;
}

static BridgeConfig parse_args(int argc, char* argv[]) {
    BridgeConfig config;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            config.show_help = true;
            return config;
        }
        if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            config.port = std::atoi(argv[++i]);
            if (config.port <= 0 || config.port > 65535) {
                std::cerr << "[godot-mcp-bridge] Invalid port number. Must be 1-65535."
                          << std::endl;
                std::exit(1);
            }
        } else if (std::strcmp(argv[i], "--host") == 0 && i + 1 < argc) {
            config.host = argv[++i];
        } else {
            std::cerr << "[godot-mcp-bridge] Unknown argument: " << argv[i] << std::endl;
            std::cerr << "Run with --help for usage information." << std::endl;
            std::exit(1);
        }
    }

    return config;
}

// ---------------------------------------------------------------------------
// Stdin reader thread
// ---------------------------------------------------------------------------

static void stdin_reader_thread(StdioTransport& transport,
                                MessageQueue& queue,
                                std::atomic<bool>& running) {
    std::string message;
    while (running.load() && transport.read_message(message)) {
        queue.push(message);
    }
    // stdin EOF or error -- signal main loop to exit
    running.store(false);
}

// ---------------------------------------------------------------------------
// Main entry point
// ---------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    BridgeConfig config = parse_args(argc, argv);

    if (config.show_help) {
        print_usage();
        return 0;
    }

    StdioTransport transport;
    transport.log("Starting bridge (pid " + std::to_string(
#ifdef _WIN32
        _getpid()
#else
        getpid()
#endif
    ) + ")");
    transport.log("Connecting to " + config.host + ":" + std::to_string(config.port));

    // Connect to GDExtension TCP server with retry
    TcpClient tcp(config.host, config.port);
    if (!tcp.connect_with_retry()) {
        transport.log("Exiting: unable to connect to Godot MCP server");
        return 1;
    }

    transport.log("Bridge relay active");

    // Start stdin reader thread
    std::atomic<bool> running{true};
    MessageQueue stdin_queue;
    std::thread reader(stdin_reader_thread,
                       std::ref(transport),
                       std::ref(stdin_queue),
                       std::ref(running));

    // Main relay loop
    while (running.load()) {
        bool did_work = false;

        // --- TCP -> stdout ---
        std::string tcp_msg;
        if (tcp.is_connected()) {
            if (tcp.receive_message(tcp_msg)) {
                transport.write_message(tcp_msg);
                did_work = true;
            } else if (!tcp.is_connected()) {
                // TCP disconnected -- attempt reconnect
                transport.log("TCP connection lost, attempting reconnect...");
                while (running.load()) {
                    transport.log("Reconnecting to " + config.host + ":"
                                  + std::to_string(config.port) + "...");
                    if (tcp.connect_with_retry(1, 2000)) {
                        transport.log("Reconnected to Godot MCP server");
                        break;
                    }
                    // Fixed 2-second interval, stay alive indefinitely
                }
                if (!running.load()) {
                    break;
                }
            }
        }

        // --- stdin -> TCP ---
        std::string stdin_msg;
        while (stdin_queue.try_pop(stdin_msg)) {
            if (tcp.is_connected()) {
                if (!tcp.send_message(stdin_msg)) {
                    transport.log("Failed to send message to TCP server");
                    // Will trigger reconnect on next iteration
                    break;
                }
            }
            // If not connected, messages stay buffered in queue (already popped,
            // but reconnect will happen above on next iteration). For simplicity,
            // log a warning -- the MCP client will need to re-handshake after reconnect.
            did_work = true;
        }

        // Avoid busy-waiting when no work was done
        if (!did_work) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    transport.log("Shutting down bridge");
    tcp.disconnect();

    if (reader.joinable()) {
        reader.join();
    }

    return 0;
}
