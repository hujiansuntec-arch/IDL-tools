// Example client usage for KeyValueStore
#include "keyvaluestore_socket.hpp"
#include <iostream>

int main() {
    ipc::KeyValueStoreClient client;

    if (!client.connect("127.0.0.1", 8888)) {
        std::cerr << "Failed to connect to server" << std::endl;
        return 1;
    }

    // Call set
    auto result = client.set("example", "example");
    std::cout << "Result: " << result << std::endl;

    return 0;
}