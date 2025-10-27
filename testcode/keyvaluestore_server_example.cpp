// Example server implementation for KeyValueStore
#include "keyvaluestore_socket.hpp"
#include <iostream>

using namespace ipc;

class MyKeyValueStoreServer : public ipc::KeyValueStoreServer {
protected:
    bool onset(const std::string& key, const std::string& value) override {
        // TODO: Implement set
        std::cout << "set called" << std::endl;
        return bool();
    }

    std::string onget(const std::string& key) override {
        // TODO: Implement get
        std::cout << "get called" << std::endl;
        return "result";
    }

    bool onremove(const std::string& key) override {
        // TODO: Implement remove
        std::cout << "remove called" << std::endl;
        return bool();
    }

    bool onexists(const std::string& key) override {
        // TODO: Implement exists
        std::cout << "exists called" << std::endl;
        return bool();
    }

    int64_t oncount() override {
        // TODO: Implement count
        std::cout << "count called" << std::endl;
        return int64_t();
    }

    void onclear() override {
        // TODO: Implement clear
        std::cout << "clear called" << std::endl;
    }

    int64_t onbatchSet(std::vector<KeyValue> items) override {
        // TODO: Implement batchSet
        std::cout << "batchSet called" << std::endl;
        return int64_t();
    }

    void onbatchGet(std::vector<std::string> keys, std::vector<std::string>& values, std::vector<OperationStatus>& status) override {
        // TODO: Implement batchGet
        std::cout << "batchGet called" << std::endl;
    }

};

int main() {
    MyKeyValueStoreServer server;

    if (!server.start(8888)) {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }

    std::cout << "Server started on port 8888" << std::endl;
    server.run();

    return 0;
}