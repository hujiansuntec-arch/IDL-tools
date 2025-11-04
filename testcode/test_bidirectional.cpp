#include "keyvaluestore_socket.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>

using namespace ipc;

// 客户端实现 - 重写回调方法
class TestClient : public KeyValueStoreClient {
private:
    std::atomic<int> callback_count_{0};
    
public:
    // 重写回调方法以接收服务器推送
    void onKeyChanged(ChangeEvent event) override {
        callback_count_++;
        std::cout << "\n[客户端] 📢 收到回调 #" << callback_count_ 
                  << " - onKeyChanged:" << std::endl;
        std::cout << "  类型: " << static_cast<int>(event.eventType) << std::endl;
        std::cout << "  键: " << event.key << std::endl;
        std::cout << "  旧值: " << event.oldValue << std::endl;
        std::cout << "  新值: " << event.newValue << std::endl;
        std::cout << "  时间戳: " << event.timestamp << std::endl;
    }
    
    void onBatchChanged(std::vector<ChangeEvent> events) override {
        callback_count_++;
        std::cout << "\n[客户端] 📢 收到回调 #" << callback_count_ 
                  << " - onBatchChanged: " << events.size() << " 个事件" << std::endl;
        for (size_t i = 0; i < events.size(); i++) {
            std::cout << "  事件[" << i << "]: key=" << events[i].key 
                      << ", newValue=" << events[i].newValue << std::endl;
        }
    }
    
    void onConnectionStatus(bool connected) override {
        callback_count_++;
        std::cout << "\n[客户端] 📢 收到回调 #" << callback_count_ 
                  << " - onConnectionStatus: " 
                  << (connected ? "已连接" : "已断开") << std::endl;
    }
    
    int getCallbackCount() const { return callback_count_; }
};

// 服务端实现
class TestServer : public KeyValueStoreServer {
private:
    std::map<std::string, std::string> store_;
    std::mutex store_mutex_;
    
public:
    bool onset(const std::string& key, const std::string& value) override {
        std::lock_guard<std::mutex> lock(store_mutex_);
        std::cout << "[服务端] ✍️  set: " << key << " = " << value << std::endl;
        
        std::string oldValue = store_[key];
        store_[key] = value;
        
        // 推送变更回调给所有客户端
        ChangeEvent event;
        event.eventType = oldValue.empty() ? ChangeEventType::KEY_ADDED : ChangeEventType::KEY_UPDATED;
        event.key = key;
        event.oldValue = oldValue;
        event.newValue = value;
        event.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
        
        std::cout << "[服务端] 📤 推送 onKeyChanged 回调..." << std::endl;
        push_onKeyChanged(event);
        
        return true;
    }
    
    std::string onget(const std::string& key) override {
        std::lock_guard<std::mutex> lock(store_mutex_);
        std::cout << "[服务端] 🔍 get: " << key << std::endl;
        return store_[key];
    }
    
    bool onremove(const std::string& key) override {
        std::lock_guard<std::mutex> lock(store_mutex_);
        std::cout << "[服务端] 🗑️  remove: " << key << std::endl;
        
        if (store_.find(key) != store_.end()) {
            std::string oldValue = store_[key];
            store_.erase(key);
            
            // 推送删除回调
            ChangeEvent event;
            event.eventType = ChangeEventType::KEY_REMOVED;
            event.key = key;
            event.oldValue = oldValue;
            event.newValue = "";
            event.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
            
            std::cout << "[服务端] 📤 推送 onKeyChanged 回调（删除）..." << std::endl;
            push_onKeyChanged(event);
            return true;
        }
        return false;
    }
    
    bool onexists(const std::string& key) override {
        std::lock_guard<std::mutex> lock(store_mutex_);
        return store_.find(key) != store_.end();
    }
    
    int64_t oncount() override {
        std::lock_guard<std::mutex> lock(store_mutex_);
        return store_.size();
    }
    
    void onclear() override {
        std::lock_guard<std::mutex> lock(store_mutex_);
        std::cout << "[服务端] 🧹 clear" << std::endl;
        store_.clear();
        
        // 推送清空回调
        ChangeEvent event;
        event.eventType = ChangeEventType::STORE_CLEARED;
        event.key = "";
        event.oldValue = "";
        event.newValue = "";
        event.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
        
        std::cout << "[服务端] 📤 推送 onKeyChanged 回调（清空）..." << std::endl;
        push_onKeyChanged(event);
    }
    
    int64_t onbatchSet(std::vector<KeyValue> items) override {
        std::lock_guard<std::mutex> lock(store_mutex_);
        std::cout << "[服务端] 📦 batchSet: " << items.size() << " 个项目" << std::endl;
        
        std::vector<ChangeEvent> events;
        for (const auto& item : items) {
            std::string oldValue = store_[item.key];
            store_[item.key] = item.value;
            
            ChangeEvent event;
            event.eventType = oldValue.empty() ? ChangeEventType::KEY_ADDED : ChangeEventType::KEY_UPDATED;
            event.key = item.key;
            event.oldValue = oldValue;
            event.newValue = item.value;
            event.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
            events.push_back(event);
        }
        
        // 推送批量变更回调
        std::cout << "[服务端] 📤 推送 onBatchChanged 回调..." << std::endl;
        push_onBatchChanged(events);
        
        return items.size();
    }
    
    void onbatchGet(std::vector<std::string> keys, 
                    std::vector<std::string>& values, 
                    std::vector<OperationStatus>& status) override {
        std::lock_guard<std::mutex> lock(store_mutex_);
        std::cout << "[服务端] 📦 batchGet: " << keys.size() << " 个键" << std::endl;
        
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
};

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "UDP双向通信测试" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    // 启动服务器
    TestServer server;
    if (!server.start(8888)) {
        std::cerr << "❌ 服务器启动失败" << std::endl;
        return 1;
    }
    std::cout << "✅ 服务器已启动在端口 8888\n" << std::endl;
    
    // 在单独线程中运行服务器
    std::thread server_thread([&server]() {
        server.run();
    });
    
    // 等待服务器启动
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 连接客户端
    TestClient client;
    if (!client.connect("127.0.0.1", 8888)) {
        std::cerr << "❌ 客户端连接失败" << std::endl;
        server.stop();
        server_thread.join();
        return 1;
    }
    std::cout << "✅ 客户端已连接\n" << std::endl;
    
    // 等待listener线程启动
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 测试1: 基本的set/get操作（会触发回调）
    std::cout << "\n--- 测试1: 基本 set/get 操作 ---" << std::endl;
    bool result = client.set("name", "Alice");
    std::cout << "set结果: " << (result ? "成功" : "失败") << std::endl;
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));  // 等待回调
    
    std::string value = client.get("name");
    std::cout << "get结果: " << value << std::endl;
    
    // 测试2: 多次set操作（触发多个回调）
    std::cout << "\n--- 测试2: 连续 set 操作 ---" << std::endl;
    client.set("age", "25");
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    client.set("city", "Beijing");
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    client.set("name", "Bob");  // 更新已有键
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    // 测试3: 批量操作（触发批量回调）
    std::cout << "\n--- 测试3: 批量 set 操作 ---" << std::endl;
    std::vector<KeyValue> items;
    items.push_back({"country", "China"});
    items.push_back({"language", "Chinese"});
    items.push_back({"hobby", "Coding"});
    
    int64_t count = client.batchSet(items);
    std::cout << "batchSet结果: " << count << " 个项目" << std::endl;
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 测试4: 删除操作（触发删除回调）
    std::cout << "\n--- 测试4: 删除操作 ---" << std::endl;
    bool removed = client.remove("age");
    std::cout << "remove结果: " << (removed ? "成功" : "失败") << std::endl;
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 测试5: 查询操作
    std::cout << "\n--- 测试5: 查询操作 ---" << std::endl;
    int64_t total = client.count();
    std::cout << "总键数: " << total << std::endl;
    
    bool exists = client.exists("name");
    std::cout << "name存在: " << (exists ? "是" : "否") << std::endl;
    
    // 测试6: 批量获取
    std::cout << "\n--- 测试6: 批量获取 ---" << std::endl;
    std::vector<std::string> keys = {"name", "city", "country", "nonexistent"};
    std::vector<std::string> values;
    std::vector<OperationStatus> statuses;
    
    client.batchGet(keys, values, statuses);
    for (size_t i = 0; i < keys.size(); i++) {
        std::cout << "  " << keys[i] << " = " << values[i] 
                  << " (状态: " << static_cast<int>(statuses[i]) << ")" << std::endl;
    }
    
    // 测试7: 服务器主动推送连接状态回调
    std::cout << "\n--- 测试7: 服务器主动推送 ---" << std::endl;
    server.push_onConnectionStatus(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 测试8: 清空操作
    std::cout << "\n--- 测试8: 清空操作 ---" << std::endl;
    client.clear();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    int64_t final_count = client.count();
    std::cout << "清空后键数: " << final_count << std::endl;
    
    // 统计结果
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "\n========================================" << std::endl;
    std::cout << "测试完成！" << std::endl;
    std::cout << "客户端收到的回调总数: " << client.getCallbackCount() << std::endl;
    std::cout << "已知客户端数量: " << server.getClientCount() << std::endl;
    std::cout << "========================================" << std::endl;
    
    // 清理
    client.stopListening();
    server.stop();
    server_thread.join();
    
    return 0;
}
