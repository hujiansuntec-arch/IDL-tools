// 测试客户端 - 支持 RPC 调用和接收 Callback
#include "keyvaluestore_socket.hpp"
#include <iostream>
#include <thread>
#include <chrono>

using namespace ipc;

class TestKeyValueStoreClient : public KeyValueStoreClient {
private:
    std::string client_name_;
    
protected:
    // 实现 callback 方法
    void onKeyChanged(ChangeEvent event) override {
        std::cout << "[" << client_name_ << "] 📢 收到 callback: onKeyChanged" << std::endl;
        std::cout << "  事件类型: ";
        switch (event.eventType) {
            case ChangeEventType::KEY_ADDED:
                std::cout << "KEY_ADDED";
                break;
            case ChangeEventType::KEY_UPDATED:
                std::cout << "KEY_UPDATED";
                break;
            case ChangeEventType::KEY_REMOVED:
                std::cout << "KEY_REMOVED";
                break;
            case ChangeEventType::STORE_CLEARED:
                std::cout << "STORE_CLEARED";
                break;
        }
        std::cout << std::endl;
        std::cout << "  键: " << event.key << std::endl;
        if (!event.oldValue.empty()) {
            std::cout << "  旧值: " << event.oldValue << std::endl;
        }
        if (!event.newValue.empty()) {
            std::cout << "  新值: " << event.newValue << std::endl;
        }
        std::cout << std::endl;
    }
    
    void onBatchChanged(std::vector<ChangeEvent> events) override {
        std::cout << "[" << client_name_ << "] 📢 收到 callback: onBatchChanged" << std::endl;
        std::cout << "  变更数量: " << events.size() << std::endl;
        for (size_t i = 0; i < events.size(); i++) {
            std::cout << "  [" << (i+1) << "] " << events[i].key << " = " << events[i].newValue << std::endl;
        }
        std::cout << std::endl;
    }
    
    void onConnectionStatus(bool connected) override {
        std::cout << "[" << client_name_ << "] 📢 收到 callback: onConnectionStatus" << std::endl;
        std::cout << "  状态: " << (connected ? "已连接" : "已断开") << std::endl;
        std::cout << std::endl;
    }
    
public:
    TestKeyValueStoreClient(const std::string& name) : client_name_(name) {}
    
    const std::string& getName() const { return client_name_; }
};

int main(int argc, char* argv[]) {
    std::string client_name = "Client";
    if (argc > 1) {
        client_name = argv[1];
    }
    
    TestKeyValueStoreClient client(client_name);
    
    std::cout << "🔌 [" << client_name << "] 连接服务器..." << std::endl;
    if (!client.connect("127.0.0.1", 8888)) {
        std::cerr << "❌ 连接失败" << std::endl;
        return 1;
    }
    std::cout << "✅ [" << client_name << "] 连接成功" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    // 启动异步监听线程（接收服务端推送的 callback）
    client.startListening();
    std::cout << "👂 [" << client_name << "] 开始监听 callback..." << std::endl;
    std::cout << std::endl;
    
    // 等待接收连接状态 callback
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    // 测试 RPC 调用
    std::cout << "========== 测试 RPC 调用 ==========" << std::endl;
    std::cout << std::endl;
    
    // 1. set 操作
    std::cout << "[" << client_name << "] 📤 调用 RPC: set(name, Alice)" << std::endl;
    bool result = client.set("name", "Alice");
    std::cout << "  返回: " << (result ? "成功" : "失败") << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // 2. get 操作
    std::cout << "[" << client_name << "] 📤 调用 RPC: get(name)" << std::endl;
    std::string value = client.get("name");
    std::cout << "  返回: " << value << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // 3. batchSet 操作
    std::cout << "[" << client_name << "] 📤 调用 RPC: batchSet(3 items)" << std::endl;
    std::vector<KeyValue> items;
    KeyValue kv1, kv2, kv3;
    kv1.key = "city"; kv1.value = "Beijing";
    kv2.key = "country"; kv2.value = "China";
    kv3.key = "age"; kv3.value = "25";
    items.push_back(kv1);
    items.push_back(kv2);
    items.push_back(kv3);
    
    int64_t count = client.batchSet(items);
    std::cout << "  返回: " << count << " 项已设置" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // 4. batchGet 操作
    std::cout << "[" << client_name << "] 📤 调用 RPC: batchGet([name, city, age])" << std::endl;
    std::vector<std::string> keys = {"name", "city", "age"};
    std::vector<std::string> values;
    std::vector<OperationStatus> status;
    
    client.batchGet(keys, values, status);
    std::cout << "  返回:" << std::endl;
    for (size_t i = 0; i < keys.size(); i++) {
        std::cout << "    " << keys[i] << " = " << values[i] 
                  << " (状态: " << (status[i] == OperationStatus::SUCCESS ? "成功" : "未找到") << ")" << std::endl;
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // 5. count 操作
    std::cout << "[" << client_name << "] 📤 调用 RPC: count()" << std::endl;
    int64_t total = client.count();
    std::cout << "  返回: " << total << " 个键" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // 6. remove 操作
    std::cout << "[" << client_name << "] 📤 调用 RPC: remove(age)" << std::endl;
    result = client.remove("age");
    std::cout << "  返回: " << (result ? "成功" : "失败") << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // 7. clear 操作
    std::cout << "[" << client_name << "] 📤 调用 RPC: clear()" << std::endl;
    client.clear();
    std::cout << "  完成" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    std::cout << std::endl;
    std::cout << "========== 测试完成 ==========" << std::endl;
    std::cout << "[" << client_name << "] 保持连接 5 秒，等待其他客户端的操作..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    client.stopListening();
    std::cout << "[" << client_name << "] 断开连接" << std::endl;
    
    return 0;
}
