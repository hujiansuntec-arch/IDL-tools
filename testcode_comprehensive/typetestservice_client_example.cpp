// Example client usage for TypeTestService
#include "typetestservice_socket.hpp"
#include <iostream>

int main() {
    ipc::TypeTestServiceClient client;

    if (!client.connect("127.0.0.1", 8888)) {
        std::cerr << "Failed to connect to server" << std::endl;
        return 1;
    }

    // Call testIntegers
    auto result = client.testIntegers(/* value */, /* value */, /* value */, /* value */, /* value */, /* value */, /* value */, /* value */);
    std::cout << "Result: " << result << std::endl;

    return 0;
}