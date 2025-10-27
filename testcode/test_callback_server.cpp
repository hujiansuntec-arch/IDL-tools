// 测试服务端 - 支持 RPC 和 Callback 推送
#include "keyvaluestore_socket.hpp"
#include <iostream>
#include <map>
#include <thread>
#include <chrono>

using namespace ipc;

class TestKeyValueStoreServer : public KeyValueStoreServer {
private:
    std::map<std::string, std::string> store_;
    
protected:
    bool onset(const std::string& key, const std::string& value) override {
        std::cout << "[Server] set: " << key << " = " << value << std::endl;
        
        std::string oldValue = store_[key];
        store_[key] = value;
        
        // 推送 callback 通知所有客户端
        ChangeEvent event;
        event.eventType = oldValue.empty() ? ChangeEventType::KEY_ADDED : ChangeEventType::KEY_UPDATED;
        event.key = key;
        event.oldValue = oldValue;
        event.newValue = value;
        event.timestamp = std::time(nullptr);
        
        push_onKeyChanged(event);
        std::cout << "[Server] 📢 推送 callback: onKeyChanged" << std::endl;
        
        return true;
    }

    std::string onget(const std::string& key) override {
        std::cout << "[Server] get: " << key << std::endl;
        
        if (store_.find(key) != store_.end()) {
            return store_[key];
        }
        return "";
    }

    bool onremove(const std::string& key) override {
        std::cout << "[Server] remove: " << key << std::endl;
        
        if (store_.find(key) != store_.end()) {
            std::string oldValue = store_[key];
            store_.erase(key);
            
            // 推送 callback
            ChangeEvent event;
            event.eventType = ChangeEventType::KEY_REMOVED;
            event.key = key;
            event.oldValue = oldValue;
            event.newValue = "";
            event.timestamp = std::time(nullptr);
            
            push_onKeyChanged(event);
            std::cout << "[Server] 📢 推送 callback: onKeyChanged (removed)" << std::endl;
            
            return true;
        }
        return false;
    }

    bool onexists(const std::string& key) override {
        return store_.find(key) != store_.end();
    }

    int64_t oncount() override {
        return store_.size();
    }

    void onclear() override {
        std::cout << "[Server] clear all" << std::endl;
        store_.clear();
        
        // 推送 callback
        ChangeEvent event;
        event.eventType = ChangeEventType::STORE_CLEARED;
        event.key = "";
        event.oldValue = "";
        event.newValue = "";
        event.timestamp = std::time(nullptr);
        
        push_onKeyChanged(event);
        std::cout << "[Server] 📢 推送 callback: onKeyChanged (cleared)" << std::endl;
    }

    int64_t onbatchSet(std::vector<KeyValue> items) override {
        std::cout << "[Server] batchSet: " << items.size() << " items" << std::endl;
        
        std::vector<ChangeEvent> events;
        
        for (const auto& item : items) {
            std::string oldValue = store_[item.key];
            store_[item.key] = item.value;
            
            ChangeEvent event;
            event.eventType = oldValue.empty() ? ChangeEventType::KEY_ADDED : ChangeEventType::KEY_UPDATED;
            event.key = item.key;
            event.oldValue = oldValue;
            event.newValue = item.value;
            event.timestamp = std::time(nullptr);
            events.push_back(event);
        }
        
        // 推送批量 callback
        push_onBatchChanged(events);
        std::cout << "[Server] 📢 推送 callback: onBatchChanged (" << events.size() << " changes)" << std::endl;
        
        return items.size();
    }

    void onbatchGet(std::vector<std::string> keys, std::vector<std::string>& values, std::vector<OperationStatus>& status) override {
        std::cout << "[Server] batchGet: " << keys.size() << " keys" << std::endl;
        
        values.clear();
        status.clear();
        
        for (const auto& key : keys) {
            if (store_.find(key) != store_.end()) {
                values.push_back(store_[key]);
                status.push_back(OperationStatus::SUCCESS);
            } else {
                values.push_back("");
                status.push_back(OperationStatus::KEY_NOT_FOUND);
            }
        }
    }
    
    void onClientConnected(int client_fd) override {
        std::cout << "[Server] ✅ 客户端连接: fd=" << client_fd << std::endl;
        
        // 向新连接的客户端推送连接状态 callback
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        push_onConnectionStatus(true);
        std::cout << "[Server] 📢 推送 callback: onConnectionStatus (connected)" << std::endl;
    }

    void onClientDisconnected(int client_fd) override {
        std::cout << "[Server] ❌ 客户端断开: fd=" << client_fd << std::endl;
    }
};

int main() {
    TestKeyValueStoreServer server;

    if (!server.start(8888)) {
        std::cerr << "❌ 启动服务器失败" << std::endl;
        return 1;
    }

    std::cout << "🚀 服务器启动成功 (端口 8888)" << std::endl;
    std::cout << "等待客户端连接..." << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    server.run();

    return 0;
}
