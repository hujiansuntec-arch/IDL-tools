// Example client usage for SchoolService
#include "schoolservice_socket.hpp"
#include <iostream>

int main() {
    ipc::SchoolServiceClient client;

    if (!client.connect("127.0.0.1", 8888)) {
        std::cerr << "Failed to connect to server" << std::endl;
        return 1;
    }

    // Call addStudent
    auto result = client.addStudent(/* value */);
    std::cout << "Result: " << result << std::endl;

    return 0;
}