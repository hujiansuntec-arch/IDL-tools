#ifndef KEYVALUESTORE_SOCKET_HPP
#define KEYVALUESTORE_SOCKET_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include <chrono>
#include <condition_variable>
#include <queue>
#include <algorithm>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

namespace ipc {

#ifndef IPC_BYTE_BUFFER_DEFINED
#define IPC_BYTE_BUFFER_DEFINED
// Serialization helpers
class ByteBuffer {
private:
    std::vector<uint8_t> data_;

public:
    void writeUint32(uint32_t value) {
        data_.push_back((value >> 24) & 0xFF);
        data_.push_back((value >> 16) & 0xFF);
        data_.push_back((value >> 8) & 0xFF);
        data_.push_back(value & 0xFF);
    }

    void writeInt32(int32_t value) {
        writeUint32(static_cast<uint32_t>(value));
    }
    
    void writeUint64(uint64_t value) {
        writeUint32(static_cast<uint32_t>(value >> 32));
        writeUint32(static_cast<uint32_t>(value & 0xFFFFFFFF));
    }
    
    void writeInt64(int64_t value) {
        writeUint64(static_cast<uint64_t>(value));
    }
    
    void writeUint16(uint16_t value) {
        data_.push_back((value >> 8) & 0xFF);
        data_.push_back(value & 0xFF);
    }
    
    void writeInt16(int16_t value) {
        writeUint16(static_cast<uint16_t>(value));
    }
    
    void writeUint8(uint8_t value) {
        data_.push_back(value);
    }
    
    void writeInt8(int8_t value) {
        data_.push_back(static_cast<uint8_t>(value));
    }
    
    void writeChar(char value) {
        data_.push_back(static_cast<uint8_t>(value));
    }

    void writeBool(bool value) {
        data_.push_back(value ? 1 : 0);
    }
    
    void writeDouble(double value) {
        uint64_t bits;
        std::memcpy(&bits, &value, sizeof(double));
        writeUint32(static_cast<uint32_t>(bits >> 32));
        writeUint32(static_cast<uint32_t>(bits & 0xFFFFFFFF));
    }
    
    void writeFloat(float value) {
        uint32_t bits;
        std::memcpy(&bits, &value, sizeof(float));
        writeUint32(bits);
    }

    void writeString(const std::string& str) {
        writeUint32(str.size());
        data_.insert(data_.end(), str.begin(), str.end());
    }

    void writeStringVector(const std::vector<std::string>& vec) {
        writeUint32(vec.size());
        for (const auto& item : vec) {
            writeString(item);
        }
    }

    const uint8_t* data() const { return data_.data(); }
    size_t size() const { return data_.size(); }
    void clear() { data_.clear(); }
};

class ByteReader {
private:
    const uint8_t* data_;
    size_t size_;
    size_t pos_;

public:
    ByteReader(const uint8_t* data, size_t size) : data_(data), size_(size), pos_(0) {}

    bool canRead(size_t bytes) const {
        return pos_ + bytes <= size_;
    }

    uint32_t readUint32() {
        if (!canRead(4)) throw std::runtime_error("Buffer underflow");
        uint32_t value = (static_cast<uint32_t>(data_[pos_]) << 24) | 
                         (static_cast<uint32_t>(data_[pos_+1]) << 16) |
                         (static_cast<uint32_t>(data_[pos_+2]) << 8) | 
                         static_cast<uint32_t>(data_[pos_+3]);
        pos_ += 4;
        return value;
    }

    int32_t readInt32() {
        return static_cast<int32_t>(readUint32());
    }
    
    uint64_t readUint64() {
        uint32_t high = readUint32();
        uint32_t low = readUint32();
        return (static_cast<uint64_t>(high) << 32) | low;
    }
    
    int64_t readInt64() {
        return static_cast<int64_t>(readUint64());
    }
    
    uint16_t readUint16() {
        if (!canRead(2)) throw std::runtime_error("Buffer underflow");
        uint16_t value = (static_cast<uint16_t>(data_[pos_]) << 8) | 
                         static_cast<uint16_t>(data_[pos_+1]);
        pos_ += 2;
        return value;
    }
    
    int16_t readInt16() {
        return static_cast<int16_t>(readUint16());
    }
    
    uint8_t readUint8() {
        if (!canRead(1)) throw std::runtime_error("Buffer underflow");
        return data_[pos_++];
    }
    
    int8_t readInt8() {
        return static_cast<int8_t>(readUint8());
    }
    
    char readChar() {
        if (!canRead(1)) throw std::runtime_error("Buffer underflow");
        return static_cast<char>(data_[pos_++]);
    }

    bool readBool() {
        if (!canRead(1)) throw std::runtime_error("Buffer underflow");
        return data_[pos_++] != 0;
    }
    
    double readDouble() {
        uint64_t bits = (static_cast<uint64_t>(readUint32()) << 32) | readUint32();
        double value;
        std::memcpy(&value, &bits, sizeof(double));
        return value;
    }
    
    float readFloat() {
        uint32_t bits = readUint32();
        float value;
        std::memcpy(&value, &bits, sizeof(float));
        return value;
    }

    std::string readString() {
        uint32_t len = readUint32();
        if (!canRead(len)) throw std::runtime_error("Buffer underflow");
        std::string str(reinterpret_cast<const char*>(data_ + pos_), len);
        pos_ += len;
        return str;
    }

    std::vector<std::string> readStringVector() {
        uint32_t count = readUint32();
        std::vector<std::string> vec;
        vec.reserve(count);
        for (uint32_t i = 0; i < count; i++) {
            vec.push_back(readString());
        }
        return vec;
    }

    size_t position() const { return pos_; }
};
#endif // IPC_BYTE_BUFFER_DEFINED

// Message IDs
const uint32_t MSG_SET_REQ = 1000;
const uint32_t MSG_SET_RESP = 1001;
const uint32_t MSG_GET_REQ = 1002;
const uint32_t MSG_GET_RESP = 1003;
const uint32_t MSG_REMOVE_REQ = 1004;
const uint32_t MSG_REMOVE_RESP = 1005;
const uint32_t MSG_EXISTS_REQ = 1006;
const uint32_t MSG_EXISTS_RESP = 1007;
const uint32_t MSG_COUNT_REQ = 1008;
const uint32_t MSG_COUNT_RESP = 1009;
const uint32_t MSG_CLEAR_REQ = 1010;
const uint32_t MSG_BATCHSET_REQ = 1011;
const uint32_t MSG_BATCHSET_RESP = 1012;
const uint32_t MSG_BATCHGET_REQ = 1013;
const uint32_t MSG_BATCHGET_RESP = 1014;
const uint32_t MSG_ONKEYCHANGED_REQ = 1015;
const uint32_t MSG_ONBATCHCHANGED_REQ = 1016;
const uint32_t MSG_ONCONNECTIONSTATUS_REQ = 1017;

#ifndef IPC_KEYVALUESERVICE_TYPES_DEFINED
#define IPC_KEYVALUESERVICE_TYPES_DEFINED
enum class OperationStatus {
    SUCCESS,
    KEY_NOT_FOUND,
    INVALID_KEY,
    ERROR
};

enum class ChangeEventType {
    KEY_ADDED,
    KEY_UPDATED,
    KEY_REMOVED,
    STORE_CLEARED
};

struct KeyValue {
    std::string key;
    std::string value;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeString(key);
        buffer.writeString(value);
    }

    void deserialize(ByteReader& reader) {
        key = reader.readString();
        value = reader.readString();
    }
};

struct ChangeEvent {
    ChangeEventType eventType;
    std::string key;
    std::string oldValue;
    std::string newValue;
    int64_t timestamp;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeInt32(static_cast<int32_t>(eventType));
        buffer.writeString(key);
        buffer.writeString(oldValue);
        buffer.writeString(newValue);
        buffer.writeInt64(timestamp);
    }

    void deserialize(ByteReader& reader) {
        eventType = static_cast<ChangeEventType>(reader.readInt32());
        key = reader.readString();
        oldValue = reader.readString();
        newValue = reader.readString();
        timestamp = reader.readInt64();
    }
};

#endif // IPC_KEYVALUESERVICE_TYPES_DEFINED

// Message Structures
struct setRequest {
    uint32_t msg_id = MSG_SET_REQ;
    std::string key;
    std::string value;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeString(key);
        buffer.writeString(value);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        key = reader.readString();
        value = reader.readString();
    }
};

struct setResponse {
    uint32_t msg_id = MSG_SET_RESP;
    int32_t status = 0;
    bool return_value;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt32(status);
        buffer.writeBool(return_value);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        status = reader.readInt32();
        return_value = reader.readBool();
    }
};

struct getRequest {
    uint32_t msg_id = MSG_GET_REQ;
    std::string key;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeString(key);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        key = reader.readString();
    }
};

struct getResponse {
    uint32_t msg_id = MSG_GET_RESP;
    int32_t status = 0;
    std::string return_value;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt32(status);
        buffer.writeString(return_value);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        status = reader.readInt32();
        return_value = reader.readString();
    }
};

struct removeRequest {
    uint32_t msg_id = MSG_REMOVE_REQ;
    std::string key;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeString(key);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        key = reader.readString();
    }
};

struct removeResponse {
    uint32_t msg_id = MSG_REMOVE_RESP;
    int32_t status = 0;
    bool return_value;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt32(status);
        buffer.writeBool(return_value);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        status = reader.readInt32();
        return_value = reader.readBool();
    }
};

struct existsRequest {
    uint32_t msg_id = MSG_EXISTS_REQ;
    std::string key;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeString(key);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        key = reader.readString();
    }
};

struct existsResponse {
    uint32_t msg_id = MSG_EXISTS_RESP;
    int32_t status = 0;
    bool return_value;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt32(status);
        buffer.writeBool(return_value);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        status = reader.readInt32();
        return_value = reader.readBool();
    }
};

struct countRequest {
    uint32_t msg_id = MSG_COUNT_REQ;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
    }
};

struct countResponse {
    uint32_t msg_id = MSG_COUNT_RESP;
    int32_t status = 0;
    int64_t return_value;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt32(status);
        buffer.writeInt64(return_value);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        status = reader.readInt32();
        return_value = reader.readInt64();
    }
};

struct clearRequest {
    uint32_t msg_id = MSG_CLEAR_REQ;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
    }
};


struct batchSetRequest {
    uint32_t msg_id = MSG_BATCHSET_REQ;
    std::vector<KeyValue> items;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeUint32(items.size());
        for (const auto& item : items) {
            item.serialize(buffer);
        }
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        {
            uint32_t count = reader.readUint32();
            items.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                items[i].deserialize(reader);
            }
        }
    }
};

struct batchSetResponse {
    uint32_t msg_id = MSG_BATCHSET_RESP;
    int32_t status = 0;
    int64_t return_value;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt32(status);
        buffer.writeInt64(return_value);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        status = reader.readInt32();
        return_value = reader.readInt64();
    }
};

struct batchGetRequest {
    uint32_t msg_id = MSG_BATCHGET_REQ;
    std::vector<std::string> keys;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeStringVector(keys);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        keys = reader.readStringVector();
    }
};

struct batchGetResponse {
    uint32_t msg_id = MSG_BATCHGET_RESP;
    std::vector<std::string> values;
    std::vector<OperationStatus> status;
    int32_t response_status = 0;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeStringVector(values);
        buffer.writeUint32(status.size());
        for (const auto& item : status) {
            buffer.writeInt32(static_cast<int32_t>(item));
        }
        buffer.writeInt32(response_status);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        values = reader.readStringVector();
        {
            uint32_t count = reader.readUint32();
            status.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                status[i] = static_cast<OperationStatus>(reader.readInt32());
            }
        }
        response_status = reader.readInt32();
    }
};

struct onKeyChangedRequest {
    uint32_t msg_id = MSG_ONKEYCHANGED_REQ;
    ChangeEvent event;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        event.serialize(buffer);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        event.deserialize(reader);
    }
};


struct onBatchChangedRequest {
    uint32_t msg_id = MSG_ONBATCHCHANGED_REQ;
    std::vector<ChangeEvent> events;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeUint32(events.size());
        for (const auto& item : events) {
            item.serialize(buffer);
        }
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        {
            uint32_t count = reader.readUint32();
            events.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                events[i].deserialize(reader);
            }
        }
    }
};


struct onConnectionStatusRequest {
    uint32_t msg_id = MSG_ONCONNECTIONSTATUS_REQ;
    bool connected;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeBool(connected);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        connected = reader.readBool();
    }
};


#ifndef IPC_SOCKET_BASE_DEFINED
#define IPC_SOCKET_BASE_DEFINED
// Socket Base Class
class SocketBase {
protected:
    int sockfd_;
    struct sockaddr_in addr_;
    bool connected_;
    
public:
    SocketBase() : sockfd_(-1), connected_(false) {}
    
    virtual ~SocketBase() {
        if (sockfd_ >= 0) {
            close(sockfd_);
        }
    }
    
    bool isConnected() const { return connected_; }
    
    ssize_t sendData(const void* data, size_t size) {
        return send(sockfd_, data, size, 0);
    }
    
    ssize_t receiveData(void* buffer, size_t size) {
        return recv(sockfd_, buffer, size, 0);
    }
    
    static ssize_t sendDataToSocket(int fd, const void* data, size_t size) {
        return send(fd, data, size, 0);
    }
    
    static ssize_t receiveDataFromSocket(int fd, void* buffer, size_t size) {
        return recv(fd, buffer, size, 0);
    }
};
#endif // IPC_SOCKET_BASE_DEFINED

// Client Interface for KeyValueStore
class KeyValueStoreClient : public SocketBase {
private:
    std::thread listener_thread_;
    bool listening_;
    std::mutex send_mutex_;

    // Message queue for RPC responses
    struct QueuedMessage {
        uint32_t msg_id;
        std::vector<uint8_t> data;
    };
    std::queue<QueuedMessage> rpc_response_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;

public:
    KeyValueStoreClient() : listening_(false) {}

    ~KeyValueStoreClient() {
        stopListening();
    }

    // Connect to server
    bool connect(const std::string& host, uint16_t port) {
        sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd_ < 0) {
            return false;
        }

        addr_.sin_family = AF_INET;
        addr_.sin_port = htons(port);
        inet_pton(AF_INET, host.c_str(), &addr_.sin_addr);

        if (::connect(sockfd_, (struct sockaddr*)&addr_, sizeof(addr_)) < 0) {
            close(sockfd_);
            sockfd_ = -1;
            return false;
        }

        connected_ = true;
        
        // Auto-start listener thread for message reception
        startListening();
        
        return true;
    }

    // Start async listening for broadcast messages
    void startListening() {
        if (listening_ || !connected_) return;
        listening_ = true;
        listener_thread_ = std::thread([this]() {
            listenLoop();
        });
    }

    // Stop async listening
    void stopListening() {
        listening_ = false;
        if (listener_thread_.joinable()) {
            listener_thread_.join();
        }
    }

private:
    void listenLoop() {
        while (listening_ && connected_) {
            // Set recv timeout to allow periodic checking of listening_ flag
            struct timeval tv;
            tv.tv_sec = 1;
            tv.tv_usec = 0;
            setsockopt(sockfd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

            // Receive message size
            uint32_t msg_size;
            ssize_t received = recv(sockfd_, &msg_size, sizeof(msg_size), 0);

            if (received <= 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    continue; // Timeout, continue listening
                }
                break; // Connection closed or error
            }

            // Receive message data
            uint8_t recv_buffer[65536];
            ssize_t recv_size = recv(sockfd_, recv_buffer, msg_size, 0);
            if (recv_size < 0 || recv_size != msg_size) {
                break;
            }

            // Parse message ID
            if (msg_size < 4) continue;
            uint32_t msg_id = (static_cast<uint32_t>(recv_buffer[0]) << 24) |
                              (static_cast<uint32_t>(recv_buffer[1]) << 16) |
                              (static_cast<uint32_t>(recv_buffer[2]) << 8) |
                              static_cast<uint32_t>(recv_buffer[3]);

            // Check if this is a callback message (REQ) or RPC response (RESP)
            bool is_callback = isCallbackMessage(msg_id);

            if (is_callback) {
                // Handle callback directly
                handleBroadcastMessage(msg_id, recv_buffer, recv_size);
            } else {
                // Queue RPC response for RPC method to retrieve
                QueuedMessage msg;
                msg.msg_id = msg_id;
                msg.data.assign(recv_buffer, recv_buffer + recv_size);
                {
                    std::lock_guard<std::mutex> lock(queue_mutex_);
                    rpc_response_queue_.push(msg);
                }
                queue_cv_.notify_one();
            }
        }
    }

    bool isCallbackMessage(uint32_t msg_id) {
        // Check if message ID corresponds to a callback (REQ message)
        switch (msg_id) {
            case MSG_ONKEYCHANGED_REQ:
            case MSG_ONBATCHCHANGED_REQ:
            case MSG_ONCONNECTIONSTATUS_REQ:
                return true;
            default:
                return false;
        }
    }

    void handleBroadcastMessage(uint32_t msg_id, const uint8_t* data, size_t size) {
        ByteReader reader(data, size);
        
        switch (msg_id) {
            case MSG_ONKEYCHANGED_REQ: {
                onKeyChangedRequest request;
                request.deserialize(reader);
                onKeyChanged(request.event);
                break;
            }
            case MSG_ONBATCHCHANGED_REQ: {
                onBatchChangedRequest request;
                request.deserialize(reader);
                onBatchChanged(request.events);
                break;
            }
            case MSG_ONCONNECTIONSTATUS_REQ: {
                onConnectionStatusRequest request;
                request.deserialize(reader);
                onConnectionStatus(request.connected);
                break;
            }
            default:
                std::cout << "[Client] Received unknown broadcast message: " << msg_id << std::endl;
                break;
        }
    }

protected:
    // Callback methods (marked with 'callback' keyword in IDL)
    virtual void onKeyChanged(ChangeEvent event) {
        // Override to handle onKeyChanged callback from server
        std::cout << "[Client] 📢 Callback: onKeyChanged" << std::endl;
    }

    virtual void onBatchChanged(std::vector<ChangeEvent> events) {
        // Override to handle onBatchChanged callback from server
        std::cout << "[Client] 📢 Callback: onBatchChanged" << std::endl;
    }

    virtual void onConnectionStatus(bool connected) {
        // Override to handle onConnectionStatus callback from server
        std::cout << "[Client] 📢 Callback: onConnectionStatus" << std::endl;
    }

public:

    bool set(const std::string& key, const std::string& value) {
        if (!connected_) {
            return bool();
        }

        // Prepare request
        setRequest request;
        request.key = key;
        request.value = value;

        // Serialize and send request (thread-safe)
        std::lock_guard<std::mutex> lock(send_mutex_);
        ByteBuffer buffer;
        request.serialize(buffer);
        // Send message size first
        uint32_t msg_size = buffer.size();
        if (sendData(&msg_size, sizeof(msg_size)) < 0) {
            return bool();
        }
        // Send message data
        if (sendData(buffer.data(), buffer.size()) < 0) {
            return bool();
        }

        // Wait for response from queue (listener thread will queue it)
        uint32_t expected_msg_id = MSG_SET_RESP;
        QueuedMessage response_msg;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            // Wait up to 5 seconds for response
            if (!queue_cv_.wait_for(lock, std::chrono::seconds(5), [&]() {
                // Check if expected response is in queue
                std::queue<QueuedMessage> temp_queue = rpc_response_queue_;
                while (!temp_queue.empty()) {
                    if (temp_queue.front().msg_id == expected_msg_id) {
                        return true;
                    }
                    temp_queue.pop();
                }
                return false;
            })) {
                return bool(); // Timeout
            }

            // Find and remove the response message from queue
            std::queue<QueuedMessage> temp_queue;
            bool found = false;
            while (!rpc_response_queue_.empty()) {
                if (rpc_response_queue_.front().msg_id == expected_msg_id && !found) {
                    response_msg = rpc_response_queue_.front();
                    found = true;
                } else {
                    temp_queue.push(rpc_response_queue_.front());
                }
                rpc_response_queue_.pop();
            }
            rpc_response_queue_ = temp_queue;
        }

        setResponse response;
        ByteReader reader(response_msg.data.data(), response_msg.data.size());
        response.deserialize(reader);

        return response.return_value;
    }

    std::string get(const std::string& key) {
        if (!connected_) {
            return std::string();
        }

        // Prepare request
        getRequest request;
        request.key = key;

        // Serialize and send request (thread-safe)
        std::lock_guard<std::mutex> lock(send_mutex_);
        ByteBuffer buffer;
        request.serialize(buffer);
        // Send message size first
        uint32_t msg_size = buffer.size();
        if (sendData(&msg_size, sizeof(msg_size)) < 0) {
            return std::string();
        }
        // Send message data
        if (sendData(buffer.data(), buffer.size()) < 0) {
            return std::string();
        }

        // Wait for response from queue (listener thread will queue it)
        uint32_t expected_msg_id = MSG_GET_RESP;
        QueuedMessage response_msg;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            // Wait up to 5 seconds for response
            if (!queue_cv_.wait_for(lock, std::chrono::seconds(5), [&]() {
                // Check if expected response is in queue
                std::queue<QueuedMessage> temp_queue = rpc_response_queue_;
                while (!temp_queue.empty()) {
                    if (temp_queue.front().msg_id == expected_msg_id) {
                        return true;
                    }
                    temp_queue.pop();
                }
                return false;
            })) {
                return std::string(); // Timeout
            }

            // Find and remove the response message from queue
            std::queue<QueuedMessage> temp_queue;
            bool found = false;
            while (!rpc_response_queue_.empty()) {
                if (rpc_response_queue_.front().msg_id == expected_msg_id && !found) {
                    response_msg = rpc_response_queue_.front();
                    found = true;
                } else {
                    temp_queue.push(rpc_response_queue_.front());
                }
                rpc_response_queue_.pop();
            }
            rpc_response_queue_ = temp_queue;
        }

        getResponse response;
        ByteReader reader(response_msg.data.data(), response_msg.data.size());
        response.deserialize(reader);

        return response.return_value;
    }

    bool remove(const std::string& key) {
        if (!connected_) {
            return bool();
        }

        // Prepare request
        removeRequest request;
        request.key = key;

        // Serialize and send request (thread-safe)
        std::lock_guard<std::mutex> lock(send_mutex_);
        ByteBuffer buffer;
        request.serialize(buffer);
        // Send message size first
        uint32_t msg_size = buffer.size();
        if (sendData(&msg_size, sizeof(msg_size)) < 0) {
            return bool();
        }
        // Send message data
        if (sendData(buffer.data(), buffer.size()) < 0) {
            return bool();
        }

        // Wait for response from queue (listener thread will queue it)
        uint32_t expected_msg_id = MSG_REMOVE_RESP;
        QueuedMessage response_msg;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            // Wait up to 5 seconds for response
            if (!queue_cv_.wait_for(lock, std::chrono::seconds(5), [&]() {
                // Check if expected response is in queue
                std::queue<QueuedMessage> temp_queue = rpc_response_queue_;
                while (!temp_queue.empty()) {
                    if (temp_queue.front().msg_id == expected_msg_id) {
                        return true;
                    }
                    temp_queue.pop();
                }
                return false;
            })) {
                return bool(); // Timeout
            }

            // Find and remove the response message from queue
            std::queue<QueuedMessage> temp_queue;
            bool found = false;
            while (!rpc_response_queue_.empty()) {
                if (rpc_response_queue_.front().msg_id == expected_msg_id && !found) {
                    response_msg = rpc_response_queue_.front();
                    found = true;
                } else {
                    temp_queue.push(rpc_response_queue_.front());
                }
                rpc_response_queue_.pop();
            }
            rpc_response_queue_ = temp_queue;
        }

        removeResponse response;
        ByteReader reader(response_msg.data.data(), response_msg.data.size());
        response.deserialize(reader);

        return response.return_value;
    }

    bool exists(const std::string& key) {
        if (!connected_) {
            return bool();
        }

        // Prepare request
        existsRequest request;
        request.key = key;

        // Serialize and send request (thread-safe)
        std::lock_guard<std::mutex> lock(send_mutex_);
        ByteBuffer buffer;
        request.serialize(buffer);
        // Send message size first
        uint32_t msg_size = buffer.size();
        if (sendData(&msg_size, sizeof(msg_size)) < 0) {
            return bool();
        }
        // Send message data
        if (sendData(buffer.data(), buffer.size()) < 0) {
            return bool();
        }

        // Wait for response from queue (listener thread will queue it)
        uint32_t expected_msg_id = MSG_EXISTS_RESP;
        QueuedMessage response_msg;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            // Wait up to 5 seconds for response
            if (!queue_cv_.wait_for(lock, std::chrono::seconds(5), [&]() {
                // Check if expected response is in queue
                std::queue<QueuedMessage> temp_queue = rpc_response_queue_;
                while (!temp_queue.empty()) {
                    if (temp_queue.front().msg_id == expected_msg_id) {
                        return true;
                    }
                    temp_queue.pop();
                }
                return false;
            })) {
                return bool(); // Timeout
            }

            // Find and remove the response message from queue
            std::queue<QueuedMessage> temp_queue;
            bool found = false;
            while (!rpc_response_queue_.empty()) {
                if (rpc_response_queue_.front().msg_id == expected_msg_id && !found) {
                    response_msg = rpc_response_queue_.front();
                    found = true;
                } else {
                    temp_queue.push(rpc_response_queue_.front());
                }
                rpc_response_queue_.pop();
            }
            rpc_response_queue_ = temp_queue;
        }

        existsResponse response;
        ByteReader reader(response_msg.data.data(), response_msg.data.size());
        response.deserialize(reader);

        return response.return_value;
    }

    int64_t count() {
        if (!connected_) {
            return int64_t();
        }

        // Prepare request
        countRequest request;

        // Serialize and send request (thread-safe)
        std::lock_guard<std::mutex> lock(send_mutex_);
        ByteBuffer buffer;
        request.serialize(buffer);
        // Send message size first
        uint32_t msg_size = buffer.size();
        if (sendData(&msg_size, sizeof(msg_size)) < 0) {
            return int64_t();
        }
        // Send message data
        if (sendData(buffer.data(), buffer.size()) < 0) {
            return int64_t();
        }

        // Wait for response from queue (listener thread will queue it)
        uint32_t expected_msg_id = MSG_COUNT_RESP;
        QueuedMessage response_msg;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            // Wait up to 5 seconds for response
            if (!queue_cv_.wait_for(lock, std::chrono::seconds(5), [&]() {
                // Check if expected response is in queue
                std::queue<QueuedMessage> temp_queue = rpc_response_queue_;
                while (!temp_queue.empty()) {
                    if (temp_queue.front().msg_id == expected_msg_id) {
                        return true;
                    }
                    temp_queue.pop();
                }
                return false;
            })) {
                return int64_t(); // Timeout
            }

            // Find and remove the response message from queue
            std::queue<QueuedMessage> temp_queue;
            bool found = false;
            while (!rpc_response_queue_.empty()) {
                if (rpc_response_queue_.front().msg_id == expected_msg_id && !found) {
                    response_msg = rpc_response_queue_.front();
                    found = true;
                } else {
                    temp_queue.push(rpc_response_queue_.front());
                }
                rpc_response_queue_.pop();
            }
            rpc_response_queue_ = temp_queue;
        }

        countResponse response;
        ByteReader reader(response_msg.data.data(), response_msg.data.size());
        response.deserialize(reader);

        return response.return_value;
    }

    bool clear() {
        if (!connected_) {
            return false;
        }

        // Prepare request
        clearRequest request;

        // Serialize and send request (thread-safe)
        std::lock_guard<std::mutex> lock(send_mutex_);
        ByteBuffer buffer;
        request.serialize(buffer);
        // Send message size first
        uint32_t msg_size = buffer.size();
        if (sendData(&msg_size, sizeof(msg_size)) < 0) {
            return false;
        }
        // Send message data
        if (sendData(buffer.data(), buffer.size()) < 0) {
            return false;
        }

        return true;
    }

    int64_t batchSet(std::vector<KeyValue> items) {
        if (!connected_) {
            return int64_t();
        }

        // Prepare request
        batchSetRequest request;
        request.items = items;

        // Serialize and send request (thread-safe)
        std::lock_guard<std::mutex> lock(send_mutex_);
        ByteBuffer buffer;
        request.serialize(buffer);
        // Send message size first
        uint32_t msg_size = buffer.size();
        if (sendData(&msg_size, sizeof(msg_size)) < 0) {
            return int64_t();
        }
        // Send message data
        if (sendData(buffer.data(), buffer.size()) < 0) {
            return int64_t();
        }

        // Wait for response from queue (listener thread will queue it)
        uint32_t expected_msg_id = MSG_BATCHSET_RESP;
        QueuedMessage response_msg;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            // Wait up to 5 seconds for response
            if (!queue_cv_.wait_for(lock, std::chrono::seconds(5), [&]() {
                // Check if expected response is in queue
                std::queue<QueuedMessage> temp_queue = rpc_response_queue_;
                while (!temp_queue.empty()) {
                    if (temp_queue.front().msg_id == expected_msg_id) {
                        return true;
                    }
                    temp_queue.pop();
                }
                return false;
            })) {
                return int64_t(); // Timeout
            }

            // Find and remove the response message from queue
            std::queue<QueuedMessage> temp_queue;
            bool found = false;
            while (!rpc_response_queue_.empty()) {
                if (rpc_response_queue_.front().msg_id == expected_msg_id && !found) {
                    response_msg = rpc_response_queue_.front();
                    found = true;
                } else {
                    temp_queue.push(rpc_response_queue_.front());
                }
                rpc_response_queue_.pop();
            }
            rpc_response_queue_ = temp_queue;
        }

        batchSetResponse response;
        ByteReader reader(response_msg.data.data(), response_msg.data.size());
        response.deserialize(reader);

        return response.return_value;
    }

    bool batchGet(std::vector<std::string> keys, std::vector<std::string>& values, std::vector<OperationStatus>& status) {
        if (!connected_) {
            return false;
        }

        // Prepare request
        batchGetRequest request;
        request.keys = keys;

        // Serialize and send request (thread-safe)
        std::lock_guard<std::mutex> lock(send_mutex_);
        ByteBuffer buffer;
        request.serialize(buffer);
        // Send message size first
        uint32_t msg_size = buffer.size();
        if (sendData(&msg_size, sizeof(msg_size)) < 0) {
            return false;
        }
        // Send message data
        if (sendData(buffer.data(), buffer.size()) < 0) {
            return false;
        }

        // Wait for response from queue (listener thread will queue it)
        uint32_t expected_msg_id = MSG_BATCHGET_RESP;
        QueuedMessage response_msg;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            // Wait up to 5 seconds for response
            if (!queue_cv_.wait_for(lock, std::chrono::seconds(5), [&]() {
                // Check if expected response is in queue
                std::queue<QueuedMessage> temp_queue = rpc_response_queue_;
                while (!temp_queue.empty()) {
                    if (temp_queue.front().msg_id == expected_msg_id) {
                        return true;
                    }
                    temp_queue.pop();
                }
                return false;
            })) {
                return false; // Timeout
            }

            // Find and remove the response message from queue
            std::queue<QueuedMessage> temp_queue;
            bool found = false;
            while (!rpc_response_queue_.empty()) {
                if (rpc_response_queue_.front().msg_id == expected_msg_id && !found) {
                    response_msg = rpc_response_queue_.front();
                    found = true;
                } else {
                    temp_queue.push(rpc_response_queue_.front());
                }
                rpc_response_queue_.pop();
            }
            rpc_response_queue_ = temp_queue;
        }

        batchGetResponse response;
        ByteReader reader(response_msg.data.data(), response_msg.data.size());
        response.deserialize(reader);

        values = response.values;
        status = response.status;
        return response.response_status == 0;
    }

};

// Server Interface for KeyValueStore
class KeyValueStoreServer : public SocketBase {
private:
    int listen_fd_;
    bool running_;
    std::vector<int> client_fds_;
    mutable std::mutex clients_mutex_;
    std::vector<std::thread> client_threads_;

public:
    KeyValueStoreServer() : listen_fd_(-1), running_(false) {}

    ~KeyValueStoreServer() {
        stop();
    }

    // Start server
    bool start(uint16_t port) {
        listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd_ < 0) {
            return false;
        }

        int opt = 1;
        setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        addr_.sin_family = AF_INET;
        addr_.sin_addr.s_addr = INADDR_ANY;
        addr_.sin_port = htons(port);

        if (bind(listen_fd_, (struct sockaddr*)&addr_, sizeof(addr_)) < 0) {
            close(listen_fd_);
            listen_fd_ = -1;
            return false;
        }

        if (listen(listen_fd_, 10) < 0) {
            close(listen_fd_);
            listen_fd_ = -1;
            return false;
        }

        running_ = true;
        return true;
    }

    void stop() {
        running_ = false;
        
        // Close all client connections
        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            for (int fd : client_fds_) {
                close(fd);
            }
            client_fds_.clear();
        }
        
        if (listen_fd_ >= 0) {
            close(listen_fd_);
            listen_fd_ = -1;
        }
        
        // Wait for all client threads to finish
        for (auto& thread : client_threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        client_threads_.clear();
    }

    // Main server loop
    void run() {
        while (running_) {
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            int client_fd = accept(listen_fd_, (struct sockaddr*)&client_addr, &addr_len);

            if (client_fd < 0) {
                if (running_) {
                    continue;
                } else {
                    break;
                }
            }

            // Add client to list
            {
                std::lock_guard<std::mutex> lock(clients_mutex_);
                client_fds_.push_back(client_fd);
            }

            // Handle client in a new thread
            client_threads_.emplace_back([this, client_fd]() {
                handleClient(client_fd);
                
                // Remove client from list
                {
                    std::lock_guard<std::mutex> lock(clients_mutex_);
                    auto it = std::find(client_fds_.begin(), client_fds_.end(), client_fd);
                    if (it != client_fds_.end()) {
                        client_fds_.erase(it);
                    }
                }
                
                close(client_fd);
                onClientDisconnected(client_fd);
            });
        }
    }

    // Broadcast message to all connected clients (with serialization)
    // exclude_fd: optionally exclude one client from broadcast (e.g., the sender)
    template<typename T>
    void broadcast(const T& message, int exclude_fd = -1) {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        
        // Serialize message once
        ByteBuffer buffer;
        message.serialize(buffer);
        uint32_t msg_size = buffer.size();
        
        // Send to all clients except excluded one
        for (int fd : client_fds_) {
            if (fd == exclude_fd) continue; // Skip the excluded client
            // Send message size
            send(fd, &msg_size, sizeof(msg_size), 0);
            // Send message data
            send(fd, buffer.data(), buffer.size(), 0);
        }
    }

    // Get number of connected clients
    size_t getClientCount() {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        return client_fds_.size();
    }

private:
    void handleClient(int client_fd) {
        onClientConnected(client_fd);
        
        while (running_) {
            // Receive message size
            uint32_t msg_size;
            ssize_t received = recv(client_fd, &msg_size, sizeof(msg_size), 0);
            
            if (received <= 0) {
                // Client disconnected or error
                break;
            }

            // Peek message ID from the data (will be consumed by handler)
            uint8_t peek_buffer[65536];
            ssize_t peeked = recv(client_fd, peek_buffer, 4, MSG_PEEK);
            if (peeked < 4) {
                break;
            }

            uint32_t msg_id = (static_cast<uint32_t>(peek_buffer[0]) << 24) |
                              (static_cast<uint32_t>(peek_buffer[1]) << 16) |
                              (static_cast<uint32_t>(peek_buffer[2]) << 8) |
                              static_cast<uint32_t>(peek_buffer[3]);

            switch (msg_id) {
                case MSG_SET_REQ:
                    handle_set(client_fd, msg_size);
                    break;
                case MSG_GET_REQ:
                    handle_get(client_fd, msg_size);
                    break;
                case MSG_REMOVE_REQ:
                    handle_remove(client_fd, msg_size);
                    break;
                case MSG_EXISTS_REQ:
                    handle_exists(client_fd, msg_size);
                    break;
                case MSG_COUNT_REQ:
                    handle_count(client_fd, msg_size);
                    break;
                case MSG_CLEAR_REQ:
                    handle_clear(client_fd, msg_size);
                    break;
                case MSG_BATCHSET_REQ:
                    handle_batchSet(client_fd, msg_size);
                    break;
                case MSG_BATCHGET_REQ:
                    handle_batchGet(client_fd, msg_size);
                    break;
                default:
                    // Unknown message, consume the data
                    {
                        uint8_t discard[65536];
                        recv(client_fd, discard, msg_size, 0);
                    }
                    break;
            }
        }
    }

    void handle_set(int client_fd, uint32_t msg_size) {
        // Receive request data (size already read by handleClient)
        uint8_t recv_buffer[65536];
        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);
        if (recv_size < 0 || recv_size != msg_size) {
            return;
        }

        setRequest request;
        ByteReader reader(recv_buffer, recv_size);
        request.deserialize(reader);

        setResponse response;
        response.return_value = onset(request.key, request.value);

        // Serialize and send response
        ByteBuffer buffer;
        response.serialize(buffer);
        // Send response size first
        uint32_t resp_size = buffer.size();
        send(client_fd, &resp_size, sizeof(resp_size), 0);
        // Send response data
        send(client_fd, buffer.data(), buffer.size(), 0);
    }

    void handle_get(int client_fd, uint32_t msg_size) {
        // Receive request data (size already read by handleClient)
        uint8_t recv_buffer[65536];
        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);
        if (recv_size < 0 || recv_size != msg_size) {
            return;
        }

        getRequest request;
        ByteReader reader(recv_buffer, recv_size);
        request.deserialize(reader);

        getResponse response;
        response.return_value = onget(request.key);

        // Serialize and send response
        ByteBuffer buffer;
        response.serialize(buffer);
        // Send response size first
        uint32_t resp_size = buffer.size();
        send(client_fd, &resp_size, sizeof(resp_size), 0);
        // Send response data
        send(client_fd, buffer.data(), buffer.size(), 0);
    }

    void handle_remove(int client_fd, uint32_t msg_size) {
        // Receive request data (size already read by handleClient)
        uint8_t recv_buffer[65536];
        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);
        if (recv_size < 0 || recv_size != msg_size) {
            return;
        }

        removeRequest request;
        ByteReader reader(recv_buffer, recv_size);
        request.deserialize(reader);

        removeResponse response;
        response.return_value = onremove(request.key);

        // Serialize and send response
        ByteBuffer buffer;
        response.serialize(buffer);
        // Send response size first
        uint32_t resp_size = buffer.size();
        send(client_fd, &resp_size, sizeof(resp_size), 0);
        // Send response data
        send(client_fd, buffer.data(), buffer.size(), 0);
    }

    void handle_exists(int client_fd, uint32_t msg_size) {
        // Receive request data (size already read by handleClient)
        uint8_t recv_buffer[65536];
        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);
        if (recv_size < 0 || recv_size != msg_size) {
            return;
        }

        existsRequest request;
        ByteReader reader(recv_buffer, recv_size);
        request.deserialize(reader);

        existsResponse response;
        response.return_value = onexists(request.key);

        // Serialize and send response
        ByteBuffer buffer;
        response.serialize(buffer);
        // Send response size first
        uint32_t resp_size = buffer.size();
        send(client_fd, &resp_size, sizeof(resp_size), 0);
        // Send response data
        send(client_fd, buffer.data(), buffer.size(), 0);
    }

    void handle_count(int client_fd, uint32_t msg_size) {
        // Receive request data (size already read by handleClient)
        uint8_t recv_buffer[65536];
        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);
        if (recv_size < 0 || recv_size != msg_size) {
            return;
        }

        countRequest request;
        ByteReader reader(recv_buffer, recv_size);
        request.deserialize(reader);

        countResponse response;
        response.return_value = oncount();

        // Serialize and send response
        ByteBuffer buffer;
        response.serialize(buffer);
        // Send response size first
        uint32_t resp_size = buffer.size();
        send(client_fd, &resp_size, sizeof(resp_size), 0);
        // Send response data
        send(client_fd, buffer.data(), buffer.size(), 0);
    }

    void handle_clear(int client_fd, uint32_t msg_size) {
        // Receive request data (size already read by handleClient)
        uint8_t recv_buffer[65536];
        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);
        if (recv_size < 0 || recv_size != msg_size) {
            return;
        }

        clearRequest request;
        ByteReader reader(recv_buffer, recv_size);
        request.deserialize(reader);

        onclear();
    }

    void handle_batchSet(int client_fd, uint32_t msg_size) {
        // Receive request data (size already read by handleClient)
        uint8_t recv_buffer[65536];
        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);
        if (recv_size < 0 || recv_size != msg_size) {
            return;
        }

        batchSetRequest request;
        ByteReader reader(recv_buffer, recv_size);
        request.deserialize(reader);

        batchSetResponse response;
        response.return_value = onbatchSet(request.items);

        // Serialize and send response
        ByteBuffer buffer;
        response.serialize(buffer);
        // Send response size first
        uint32_t resp_size = buffer.size();
        send(client_fd, &resp_size, sizeof(resp_size), 0);
        // Send response data
        send(client_fd, buffer.data(), buffer.size(), 0);
    }

    void handle_batchGet(int client_fd, uint32_t msg_size) {
        // Receive request data (size already read by handleClient)
        uint8_t recv_buffer[65536];
        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);
        if (recv_size < 0 || recv_size != msg_size) {
            return;
        }

        batchGetRequest request;
        ByteReader reader(recv_buffer, recv_size);
        request.deserialize(reader);

        batchGetResponse response;
        onbatchGet(request.keys, response.values, response.status);

        // Serialize and send response
        ByteBuffer buffer;
        response.serialize(buffer);
        // Send response size first
        uint32_t resp_size = buffer.size();
        send(client_fd, &resp_size, sizeof(resp_size), 0);
        // Send response data
        send(client_fd, buffer.data(), buffer.size(), 0);
    }

public:
    // Callback push methods (send callbacks to clients)
    void push_onKeyChanged(ChangeEvent event, int exclude_fd = -1) {
        // Prepare callback request
        onKeyChangedRequest request;
        request.event = event;

        // Broadcast callback to all clients
        broadcast(request, exclude_fd);
    }

    void push_onBatchChanged(std::vector<ChangeEvent> events, int exclude_fd = -1) {
        // Prepare callback request
        onBatchChangedRequest request;
        request.events = events;

        // Broadcast callback to all clients
        broadcast(request, exclude_fd);
    }

    void push_onConnectionStatus(bool connected, int exclude_fd = -1) {
        // Prepare callback request
        onConnectionStatusRequest request;
        request.connected = connected;

        // Broadcast callback to all clients
        broadcast(request, exclude_fd);
    }

protected:
    // Virtual functions to be implemented by user
    virtual bool onset(const std::string& key, const std::string& value) = 0;
    virtual std::string onget(const std::string& key) = 0;
    virtual bool onremove(const std::string& key) = 0;
    virtual bool onexists(const std::string& key) = 0;
    virtual int64_t oncount() = 0;
    virtual void onclear() = 0;
    virtual int64_t onbatchSet(std::vector<KeyValue> items) = 0;
    virtual void onbatchGet(std::vector<std::string> keys, std::vector<std::string>& values, std::vector<OperationStatus>& status) = 0;

    // Optional callbacks for client connection events
    virtual void onClientConnected(int client_fd) {
        // Override this to handle client connection
    }

    virtual void onClientDisconnected(int client_fd) {
        // Override this to handle client disconnection
    }
};

} // namespace ipc

#endif // KEYVALUESTORE_SOCKET_HPP