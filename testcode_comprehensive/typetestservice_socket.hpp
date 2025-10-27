#ifndef TYPETESTSERVICE_SOCKET_HPP
#define TYPETESTSERVICE_SOCKET_HPP

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
const uint32_t MSG_TESTINTEGERS_REQ = 1000;
const uint32_t MSG_TESTINTEGERS_RESP = 1001;
const uint32_t MSG_TESTFLOATS_REQ = 1002;
const uint32_t MSG_TESTFLOATS_RESP = 1003;
const uint32_t MSG_TESTCHARANDBOOL_REQ = 1004;
const uint32_t MSG_TESTCHARANDBOOL_RESP = 1005;
const uint32_t MSG_TESTSTRING_REQ = 1006;
const uint32_t MSG_TESTSTRING_RESP = 1007;
const uint32_t MSG_TESTENUM_REQ = 1008;
const uint32_t MSG_TESTENUM_RESP = 1009;
const uint32_t MSG_TESTSTRUCT_REQ = 1010;
const uint32_t MSG_TESTSTRUCT_RESP = 1011;
const uint32_t MSG_TESTNESTEDSTRUCT_REQ = 1012;
const uint32_t MSG_TESTNESTEDSTRUCT_RESP = 1013;
const uint32_t MSG_TESTINT32VECTOR_REQ = 1014;
const uint32_t MSG_TESTINT32VECTOR_RESP = 1015;
const uint32_t MSG_TESTUINT64VECTOR_REQ = 1016;
const uint32_t MSG_TESTUINT64VECTOR_RESP = 1017;
const uint32_t MSG_TESTFLOATVECTOR_REQ = 1018;
const uint32_t MSG_TESTFLOATVECTOR_RESP = 1019;
const uint32_t MSG_TESTDOUBLEVECTOR_REQ = 1020;
const uint32_t MSG_TESTDOUBLEVECTOR_RESP = 1021;
const uint32_t MSG_TESTSTRINGVECTOR_REQ = 1022;
const uint32_t MSG_TESTSTRINGVECTOR_RESP = 1023;
const uint32_t MSG_TESTBOOLVECTOR_REQ = 1024;
const uint32_t MSG_TESTBOOLVECTOR_RESP = 1025;
const uint32_t MSG_TESTENUMVECTOR_REQ = 1026;
const uint32_t MSG_TESTENUMVECTOR_RESP = 1027;
const uint32_t MSG_TESTSTRUCTVECTOR_REQ = 1028;
const uint32_t MSG_TESTSTRUCTVECTOR_RESP = 1029;
const uint32_t MSG_TESTNESTEDSTRUCTVECTOR_REQ = 1030;
const uint32_t MSG_TESTNESTEDSTRUCTVECTOR_RESP = 1031;
const uint32_t MSG_TESTCOMPLEXDATA_REQ = 1032;
const uint32_t MSG_TESTCOMPLEXDATA_RESP = 1033;
const uint32_t MSG_TESTOUTPARAMS_REQ = 1034;
const uint32_t MSG_TESTOUTPARAMS_RESP = 1035;
const uint32_t MSG_TESTOUTVECTORS_REQ = 1036;
const uint32_t MSG_TESTOUTVECTORS_RESP = 1037;
const uint32_t MSG_TESTINOUTPARAMS_REQ = 1038;
const uint32_t MSG_TESTINOUTPARAMS_RESP = 1039;
const uint32_t MSG_ONINTEGERUPDATE_REQ = 1040;
const uint32_t MSG_ONFLOATUPDATE_REQ = 1041;
const uint32_t MSG_ONSTRUCTUPDATE_REQ = 1042;
const uint32_t MSG_ONVECTORUPDATE_REQ = 1043;
const uint32_t MSG_ONCOMPLEXUPDATE_REQ = 1044;

#ifndef IPC_TYPETEST_TYPES_DEFINED
#define IPC_TYPETEST_TYPES_DEFINED
enum class Priority {
    LOW,
    MEDIUM,
    HIGH,
    CRITICAL
};

enum class Status {
    PENDING,
    PROCESSING,
    COMPLETED,
    FAILED
};

struct IntegerTypes {
    int8_t i8;
    uint8_t u8;
    int16_t i16;
    uint16_t u16;
    int32_t i32;
    uint32_t u32;
    int64_t i64;
    uint64_t u64;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeInt8(i8);
        buffer.writeUint8(u8);
        buffer.writeInt16(i16);
        buffer.writeUint16(u16);
        buffer.writeInt32(i32);
        buffer.writeUint32(u32);
        buffer.writeInt64(i64);
        buffer.writeUint64(u64);
    }

    void deserialize(ByteReader& reader) {
        i8 = reader.readInt8();
        u8 = reader.readUint8();
        i16 = reader.readInt16();
        u16 = reader.readUint16();
        i32 = reader.readInt32();
        u32 = reader.readUint32();
        i64 = reader.readInt64();
        u64 = reader.readUint64();
    }
};

struct FloatAndCharTypes {
    float f;
    double d;
    char c;
    bool b;
    std::string str;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeFloat(f);
        buffer.writeDouble(d);
        buffer.writeChar(c);
        buffer.writeBool(b);
        buffer.writeString(str);
    }

    void deserialize(ByteReader& reader) {
        f = reader.readFloat();
        d = reader.readDouble();
        c = reader.readChar();
        b = reader.readBool();
        str = reader.readString();
    }
};

struct NestedData {
    IntegerTypes integers;
    FloatAndCharTypes floats;
    Priority priority;
    Status status;

    void serialize(ByteBuffer& buffer) const {
        integers.serialize(buffer);
        floats.serialize(buffer);
        buffer.writeInt32(static_cast<int32_t>(priority));
        buffer.writeInt32(static_cast<int32_t>(status));
    }

    void deserialize(ByteReader& reader) {
        integers.deserialize(reader);
        floats.deserialize(reader);
        priority = static_cast<Priority>(reader.readInt32());
        status = static_cast<Status>(reader.readInt32());
    }
};

struct ComplexData {
    std::vector<int8_t> i8seq;
    std::vector<uint8_t> u8seq;
    std::vector<int16_t> i16seq;
    std::vector<uint16_t> u16seq;
    std::vector<int32_t> i32seq;
    std::vector<uint32_t> u32seq;
    std::vector<int64_t> i64seq;
    std::vector<uint64_t> u64seq;
    std::vector<float> fseq;
    std::vector<double> dseq;
    std::vector<char> cseq;
    std::vector<bool> bseq;
    std::vector<std::string> strseq;
    std::vector<Priority> priseq;
    std::vector<Status> stseq;
    std::vector<IntegerTypes> intseq;
    std::vector<NestedData> nestedseq;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(i8seq.size());
        for (const auto& item : i8seq) {
            buffer.writeInt8(item);
        }
        buffer.writeUint32(u8seq.size());
        for (const auto& item : u8seq) {
            buffer.writeUint8(item);
        }
        buffer.writeUint32(i16seq.size());
        for (const auto& item : i16seq) {
            buffer.writeInt16(item);
        }
        buffer.writeUint32(u16seq.size());
        for (const auto& item : u16seq) {
            buffer.writeUint16(item);
        }
        buffer.writeUint32(i32seq.size());
        for (const auto& item : i32seq) {
            buffer.writeInt32(item);
        }
        buffer.writeUint32(u32seq.size());
        for (const auto& item : u32seq) {
            buffer.writeUint32(item);
        }
        buffer.writeUint32(i64seq.size());
        for (const auto& item : i64seq) {
            buffer.writeInt64(item);
        }
        buffer.writeUint32(u64seq.size());
        for (const auto& item : u64seq) {
            buffer.writeUint64(item);
        }
        buffer.writeUint32(fseq.size());
        for (const auto& item : fseq) {
            buffer.writeFloat(item);
        }
        buffer.writeUint32(dseq.size());
        for (const auto& item : dseq) {
            buffer.writeDouble(item);
        }
        buffer.writeUint32(cseq.size());
        for (const auto& item : cseq) {
            buffer.writeChar(item);
        }
        buffer.writeUint32(bseq.size());
        for (const auto& item : bseq) {
            buffer.writeBool(item);
        }
        buffer.writeUint32(strseq.size());
        for (const auto& item : strseq) {
            buffer.writeString(item);
        }
        buffer.writeUint32(priseq.size());
        for (const auto& item : priseq) {
            buffer.writeInt32(static_cast<int32_t>(item));
        }
        buffer.writeUint32(stseq.size());
        for (const auto& item : stseq) {
            buffer.writeInt32(static_cast<int32_t>(item));
        }
        buffer.writeUint32(intseq.size());
        for (const auto& item : intseq) {
            item.serialize(buffer);
        }
        buffer.writeUint32(nestedseq.size());
        for (const auto& item : nestedseq) {
            item.serialize(buffer);
        }
    }

    void deserialize(ByteReader& reader) {
        {
            uint32_t count = reader.readUint32();
            i8seq.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                i8seq[i] = reader.readInt8();
            }
        }
        {
            uint32_t count = reader.readUint32();
            u8seq.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                u8seq[i] = reader.readUint8();
            }
        }
        {
            uint32_t count = reader.readUint32();
            i16seq.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                i16seq[i] = reader.readInt16();
            }
        }
        {
            uint32_t count = reader.readUint32();
            u16seq.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                u16seq[i] = reader.readUint16();
            }
        }
        {
            uint32_t count = reader.readUint32();
            i32seq.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                i32seq[i] = reader.readInt32();
            }
        }
        {
            uint32_t count = reader.readUint32();
            u32seq.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                u32seq[i] = reader.readUint32();
            }
        }
        {
            uint32_t count = reader.readUint32();
            i64seq.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                i64seq[i] = reader.readInt64();
            }
        }
        {
            uint32_t count = reader.readUint32();
            u64seq.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                u64seq[i] = reader.readUint64();
            }
        }
        {
            uint32_t count = reader.readUint32();
            fseq.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                fseq[i] = reader.readFloat();
            }
        }
        {
            uint32_t count = reader.readUint32();
            dseq.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                dseq[i] = reader.readDouble();
            }
        }
        {
            uint32_t count = reader.readUint32();
            cseq.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                cseq[i] = reader.readChar();
            }
        }
        {
            uint32_t count = reader.readUint32();
            bseq.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                bseq[i] = reader.readBool();
            }
        }
        {
            uint32_t count = reader.readUint32();
            strseq.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                strseq[i] = reader.readString();
            }
        }
        {
            uint32_t count = reader.readUint32();
            priseq.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                priseq[i] = static_cast<Priority>(reader.readInt32());
            }
        }
        {
            uint32_t count = reader.readUint32();
            stseq.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                stseq[i] = static_cast<Status>(reader.readInt32());
            }
        }
        {
            uint32_t count = reader.readUint32();
            intseq.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                intseq[i].deserialize(reader);
            }
        }
        {
            uint32_t count = reader.readUint32();
            nestedseq.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                nestedseq[i].deserialize(reader);
            }
        }
    }
};

#endif // IPC_TYPETEST_TYPES_DEFINED

// Message Structures
struct testIntegersRequest {
    uint32_t msg_id = MSG_TESTINTEGERS_REQ;
    int8_t i8;
    uint8_t u8;
    int16_t i16;
    uint16_t u16;
    int32_t i32;
    uint32_t u32;
    int64_t i64;
    uint64_t u64;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt8(i8);
        buffer.writeUint8(u8);
        buffer.writeInt16(i16);
        buffer.writeUint16(u16);
        buffer.writeInt32(i32);
        buffer.writeUint32(u32);
        buffer.writeInt64(i64);
        buffer.writeUint64(u64);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        i8 = reader.readInt8();
        u8 = reader.readUint8();
        i16 = reader.readInt16();
        u16 = reader.readUint16();
        i32 = reader.readInt32();
        u32 = reader.readUint32();
        i64 = reader.readInt64();
        u64 = reader.readUint64();
    }
};

struct testIntegersResponse {
    uint32_t msg_id = MSG_TESTINTEGERS_RESP;
    int32_t status = 0;
    int32_t return_value;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt32(status);
        buffer.writeInt32(return_value);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        status = reader.readInt32();
        return_value = reader.readInt32();
    }
};

struct testFloatsRequest {
    uint32_t msg_id = MSG_TESTFLOATS_REQ;
    float f;
    double d;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeFloat(f);
        buffer.writeDouble(d);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        f = reader.readFloat();
        d = reader.readDouble();
    }
};

struct testFloatsResponse {
    uint32_t msg_id = MSG_TESTFLOATS_RESP;
    int32_t status = 0;
    double return_value;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt32(status);
        buffer.writeDouble(return_value);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        status = reader.readInt32();
        return_value = reader.readDouble();
    }
};

struct testCharAndBoolRequest {
    uint32_t msg_id = MSG_TESTCHARANDBOOL_REQ;
    char c;
    bool b;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeChar(c);
        buffer.writeBool(b);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        c = reader.readChar();
        b = reader.readBool();
    }
};

struct testCharAndBoolResponse {
    uint32_t msg_id = MSG_TESTCHARANDBOOL_RESP;
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

struct testStringRequest {
    uint32_t msg_id = MSG_TESTSTRING_REQ;
    std::string str;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeString(str);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        str = reader.readString();
    }
};

struct testStringResponse {
    uint32_t msg_id = MSG_TESTSTRING_RESP;
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

struct testEnumRequest {
    uint32_t msg_id = MSG_TESTENUM_REQ;
    Priority p;
    Status s;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt32(static_cast<int32_t>(p));
        buffer.writeInt32(static_cast<int32_t>(s));
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        p = static_cast<Priority>(reader.readInt32());
        s = static_cast<Status>(reader.readInt32());
    }
};

struct testEnumResponse {
    uint32_t msg_id = MSG_TESTENUM_RESP;
    int32_t status = 0;
    Priority return_value;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt32(status);
        buffer.writeInt32(static_cast<int32_t>(return_value));
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        status = reader.readInt32();
        return_value = static_cast<Priority>(reader.readInt32());
    }
};

struct testStructRequest {
    uint32_t msg_id = MSG_TESTSTRUCT_REQ;
    IntegerTypes data;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        data.serialize(buffer);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        data.deserialize(reader);
    }
};

struct testStructResponse {
    uint32_t msg_id = MSG_TESTSTRUCT_RESP;
    int32_t status = 0;
    IntegerTypes return_value;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt32(status);
        return_value.serialize(buffer);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        status = reader.readInt32();
        return_value.deserialize(reader);
    }
};

struct testNestedStructRequest {
    uint32_t msg_id = MSG_TESTNESTEDSTRUCT_REQ;
    NestedData data;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        data.serialize(buffer);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        data.deserialize(reader);
    }
};

struct testNestedStructResponse {
    uint32_t msg_id = MSG_TESTNESTEDSTRUCT_RESP;
    int32_t status = 0;
    NestedData return_value;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt32(status);
        return_value.serialize(buffer);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        status = reader.readInt32();
        return_value.deserialize(reader);
    }
};

struct testInt32VectorRequest {
    uint32_t msg_id = MSG_TESTINT32VECTOR_REQ;
    std::vector<int32_t> seq;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeUint32(seq.size());
        for (const auto& item : seq) {
            buffer.writeInt32(item);
        }
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        {
            uint32_t count = reader.readUint32();
            seq.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                seq[i] = reader.readInt32();
            }
        }
    }
};

struct testInt32VectorResponse {
    uint32_t msg_id = MSG_TESTINT32VECTOR_RESP;
    int32_t status = 0;
    std::vector<int32_t> return_value;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt32(status);
        buffer.writeUint32(return_value.size());
        for (const auto& item : return_value) {
            buffer.writeInt32(item);
        }
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        status = reader.readInt32();
        {
            uint32_t count = reader.readUint32();
            return_value.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                return_value[i] = reader.readInt32();
            }
        }
    }
};

struct testUInt64VectorRequest {
    uint32_t msg_id = MSG_TESTUINT64VECTOR_REQ;
    std::vector<uint64_t> seq;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeUint32(seq.size());
        for (const auto& item : seq) {
            buffer.writeUint64(item);
        }
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        {
            uint32_t count = reader.readUint32();
            seq.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                seq[i] = reader.readUint64();
            }
        }
    }
};

struct testUInt64VectorResponse {
    uint32_t msg_id = MSG_TESTUINT64VECTOR_RESP;
    int32_t status = 0;
    std::vector<uint64_t> return_value;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt32(status);
        buffer.writeUint32(return_value.size());
        for (const auto& item : return_value) {
            buffer.writeUint64(item);
        }
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        status = reader.readInt32();
        {
            uint32_t count = reader.readUint32();
            return_value.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                return_value[i] = reader.readUint64();
            }
        }
    }
};

struct testFloatVectorRequest {
    uint32_t msg_id = MSG_TESTFLOATVECTOR_REQ;
    std::vector<float> seq;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeUint32(seq.size());
        for (const auto& item : seq) {
            buffer.writeFloat(item);
        }
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        {
            uint32_t count = reader.readUint32();
            seq.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                seq[i] = reader.readFloat();
            }
        }
    }
};

struct testFloatVectorResponse {
    uint32_t msg_id = MSG_TESTFLOATVECTOR_RESP;
    int32_t status = 0;
    std::vector<float> return_value;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt32(status);
        buffer.writeUint32(return_value.size());
        for (const auto& item : return_value) {
            buffer.writeFloat(item);
        }
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        status = reader.readInt32();
        {
            uint32_t count = reader.readUint32();
            return_value.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                return_value[i] = reader.readFloat();
            }
        }
    }
};

struct testDoubleVectorRequest {
    uint32_t msg_id = MSG_TESTDOUBLEVECTOR_REQ;
    std::vector<double> seq;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeUint32(seq.size());
        for (const auto& item : seq) {
            buffer.writeDouble(item);
        }
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        {
            uint32_t count = reader.readUint32();
            seq.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                seq[i] = reader.readDouble();
            }
        }
    }
};

struct testDoubleVectorResponse {
    uint32_t msg_id = MSG_TESTDOUBLEVECTOR_RESP;
    int32_t status = 0;
    std::vector<double> return_value;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt32(status);
        buffer.writeUint32(return_value.size());
        for (const auto& item : return_value) {
            buffer.writeDouble(item);
        }
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        status = reader.readInt32();
        {
            uint32_t count = reader.readUint32();
            return_value.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                return_value[i] = reader.readDouble();
            }
        }
    }
};

struct testStringVectorRequest {
    uint32_t msg_id = MSG_TESTSTRINGVECTOR_REQ;
    std::vector<std::string> seq;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeStringVector(seq);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        seq = reader.readStringVector();
    }
};

struct testStringVectorResponse {
    uint32_t msg_id = MSG_TESTSTRINGVECTOR_RESP;
    int32_t status = 0;
    std::vector<std::string> return_value;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt32(status);
        buffer.writeStringVector(return_value);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        status = reader.readInt32();
        return_value = reader.readStringVector();
    }
};

struct testBoolVectorRequest {
    uint32_t msg_id = MSG_TESTBOOLVECTOR_REQ;
    std::vector<bool> seq;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeUint32(seq.size());
        for (const auto& item : seq) {
            buffer.writeBool(item);
        }
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        {
            uint32_t count = reader.readUint32();
            seq.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                seq[i] = reader.readBool();
            }
        }
    }
};

struct testBoolVectorResponse {
    uint32_t msg_id = MSG_TESTBOOLVECTOR_RESP;
    int32_t status = 0;
    std::vector<bool> return_value;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt32(status);
        buffer.writeUint32(return_value.size());
        for (const auto& item : return_value) {
            buffer.writeBool(item);
        }
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        status = reader.readInt32();
        {
            uint32_t count = reader.readUint32();
            return_value.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                return_value[i] = reader.readBool();
            }
        }
    }
};

struct testEnumVectorRequest {
    uint32_t msg_id = MSG_TESTENUMVECTOR_REQ;
    std::vector<Priority> seq;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeUint32(seq.size());
        for (const auto& item : seq) {
            buffer.writeInt32(static_cast<int32_t>(item));
        }
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        {
            uint32_t count = reader.readUint32();
            seq.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                seq[i] = static_cast<Priority>(reader.readInt32());
            }
        }
    }
};

struct testEnumVectorResponse {
    uint32_t msg_id = MSG_TESTENUMVECTOR_RESP;
    int32_t status = 0;
    std::vector<Priority> return_value;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt32(status);
        buffer.writeUint32(return_value.size());
        for (const auto& item : return_value) {
            buffer.writeInt32(static_cast<int32_t>(item));
        }
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        status = reader.readInt32();
        {
            uint32_t count = reader.readUint32();
            return_value.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                return_value[i] = static_cast<Priority>(reader.readInt32());
            }
        }
    }
};

struct testStructVectorRequest {
    uint32_t msg_id = MSG_TESTSTRUCTVECTOR_REQ;
    std::vector<IntegerTypes> seq;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeUint32(seq.size());
        for (const auto& item : seq) {
            item.serialize(buffer);
        }
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        {
            uint32_t count = reader.readUint32();
            seq.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                seq[i].deserialize(reader);
            }
        }
    }
};

struct testStructVectorResponse {
    uint32_t msg_id = MSG_TESTSTRUCTVECTOR_RESP;
    int32_t status = 0;
    std::vector<IntegerTypes> return_value;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt32(status);
        buffer.writeUint32(return_value.size());
        for (const auto& item : return_value) {
            item.serialize(buffer);
        }
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        status = reader.readInt32();
        {
            uint32_t count = reader.readUint32();
            return_value.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                return_value[i].deserialize(reader);
            }
        }
    }
};

struct testNestedStructVectorRequest {
    uint32_t msg_id = MSG_TESTNESTEDSTRUCTVECTOR_REQ;
    std::vector<NestedData> seq;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeUint32(seq.size());
        for (const auto& item : seq) {
            item.serialize(buffer);
        }
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        {
            uint32_t count = reader.readUint32();
            seq.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                seq[i].deserialize(reader);
            }
        }
    }
};

struct testNestedStructVectorResponse {
    uint32_t msg_id = MSG_TESTNESTEDSTRUCTVECTOR_RESP;
    int32_t status = 0;
    std::vector<NestedData> return_value;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt32(status);
        buffer.writeUint32(return_value.size());
        for (const auto& item : return_value) {
            item.serialize(buffer);
        }
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        status = reader.readInt32();
        {
            uint32_t count = reader.readUint32();
            return_value.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                return_value[i].deserialize(reader);
            }
        }
    }
};

struct testComplexDataRequest {
    uint32_t msg_id = MSG_TESTCOMPLEXDATA_REQ;
    ComplexData data;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        data.serialize(buffer);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        data.deserialize(reader);
    }
};

struct testComplexDataResponse {
    uint32_t msg_id = MSG_TESTCOMPLEXDATA_RESP;
    int32_t status = 0;
    ComplexData return_value;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt32(status);
        return_value.serialize(buffer);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        status = reader.readInt32();
        return_value.deserialize(reader);
    }
};

struct testOutParamsRequest {
    uint32_t msg_id = MSG_TESTOUTPARAMS_REQ;
    int32_t input;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt32(input);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        input = reader.readInt32();
    }
};

struct testOutParamsResponse {
    uint32_t msg_id = MSG_TESTOUTPARAMS_RESP;
    int32_t status = 0;
    int8_t o_i8;
    uint8_t o_u8;
    int16_t o_i16;
    uint16_t o_u16;
    int32_t o_i32;
    uint32_t o_u32;
    int64_t o_i64;
    uint64_t o_u64;
    float o_f;
    double o_d;
    char o_c;
    bool o_b;
    std::string o_str;
    Priority o_p;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt32(status);
        buffer.writeInt8(o_i8);
        buffer.writeUint8(o_u8);
        buffer.writeInt16(o_i16);
        buffer.writeUint16(o_u16);
        buffer.writeInt32(o_i32);
        buffer.writeUint32(o_u32);
        buffer.writeInt64(o_i64);
        buffer.writeUint64(o_u64);
        buffer.writeFloat(o_f);
        buffer.writeDouble(o_d);
        buffer.writeChar(o_c);
        buffer.writeBool(o_b);
        buffer.writeString(o_str);
        buffer.writeInt32(static_cast<int32_t>(o_p));
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        status = reader.readInt32();
        o_i8 = reader.readInt8();
        o_u8 = reader.readUint8();
        o_i16 = reader.readInt16();
        o_u16 = reader.readUint16();
        o_i32 = reader.readInt32();
        o_u32 = reader.readUint32();
        o_i64 = reader.readInt64();
        o_u64 = reader.readUint64();
        o_f = reader.readFloat();
        o_d = reader.readDouble();
        o_c = reader.readChar();
        o_b = reader.readBool();
        o_str = reader.readString();
        o_p = static_cast<Priority>(reader.readInt32());
    }
};

struct testOutVectorsRequest {
    uint32_t msg_id = MSG_TESTOUTVECTORS_REQ;
    int32_t count;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt32(count);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        count = reader.readInt32();
    }
};

struct testOutVectorsResponse {
    uint32_t msg_id = MSG_TESTOUTVECTORS_RESP;
    int32_t status = 0;
    std::vector<int32_t> o_i32seq;
    std::vector<float> o_fseq;
    std::vector<std::string> o_strseq;
    std::vector<Priority> o_pseq;
    std::vector<IntegerTypes> o_structseq;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt32(status);
        buffer.writeUint32(o_i32seq.size());
        for (const auto& item : o_i32seq) {
            buffer.writeInt32(item);
        }
        buffer.writeUint32(o_fseq.size());
        for (const auto& item : o_fseq) {
            buffer.writeFloat(item);
        }
        buffer.writeStringVector(o_strseq);
        buffer.writeUint32(o_pseq.size());
        for (const auto& item : o_pseq) {
            buffer.writeInt32(static_cast<int32_t>(item));
        }
        buffer.writeUint32(o_structseq.size());
        for (const auto& item : o_structseq) {
            item.serialize(buffer);
        }
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        status = reader.readInt32();
        {
            uint32_t count = reader.readUint32();
            o_i32seq.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                o_i32seq[i] = reader.readInt32();
            }
        }
        {
            uint32_t count = reader.readUint32();
            o_fseq.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                o_fseq[i] = reader.readFloat();
            }
        }
        o_strseq = reader.readStringVector();
        {
            uint32_t count = reader.readUint32();
            o_pseq.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                o_pseq[i] = static_cast<Priority>(reader.readInt32());
            }
        }
        {
            uint32_t count = reader.readUint32();
            o_structseq.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                o_structseq[i].deserialize(reader);
            }
        }
    }
};

struct testInOutParamsRequest {
    uint32_t msg_id = MSG_TESTINOUTPARAMS_REQ;
    int32_t value;
    std::string str;
    IntegerTypes data;
    std::vector<int32_t> seq;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt32(value);
        buffer.writeString(str);
        data.serialize(buffer);
        buffer.writeUint32(seq.size());
        for (const auto& item : seq) {
            buffer.writeInt32(item);
        }
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        value = reader.readInt32();
        str = reader.readString();
        data.deserialize(reader);
        {
            uint32_t count = reader.readUint32();
            seq.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                seq[i] = reader.readInt32();
            }
        }
    }
};

struct testInOutParamsResponse {
    uint32_t msg_id = MSG_TESTINOUTPARAMS_RESP;
    int32_t status = 0;
    int32_t value;
    std::string str;
    IntegerTypes data;
    std::vector<int32_t> seq;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt32(status);
        buffer.writeInt32(value);
        buffer.writeString(str);
        data.serialize(buffer);
        buffer.writeUint32(seq.size());
        for (const auto& item : seq) {
            buffer.writeInt32(item);
        }
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        status = reader.readInt32();
        value = reader.readInt32();
        str = reader.readString();
        data.deserialize(reader);
        {
            uint32_t count = reader.readUint32();
            seq.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                seq[i] = reader.readInt32();
            }
        }
    }
};

struct onIntegerUpdateRequest {
    uint32_t msg_id = MSG_ONINTEGERUPDATE_REQ;
    int8_t i8;
    uint8_t u8;
    int32_t i32;
    int64_t i64;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeInt8(i8);
        buffer.writeUint8(u8);
        buffer.writeInt32(i32);
        buffer.writeInt64(i64);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        i8 = reader.readInt8();
        u8 = reader.readUint8();
        i32 = reader.readInt32();
        i64 = reader.readInt64();
    }
};


struct onFloatUpdateRequest {
    uint32_t msg_id = MSG_ONFLOATUPDATE_REQ;
    float f;
    double d;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeFloat(f);
        buffer.writeDouble(d);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        f = reader.readFloat();
        d = reader.readDouble();
    }
};


struct onStructUpdateRequest {
    uint32_t msg_id = MSG_ONSTRUCTUPDATE_REQ;
    IntegerTypes data;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        data.serialize(buffer);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        data.deserialize(reader);
    }
};


struct onVectorUpdateRequest {
    uint32_t msg_id = MSG_ONVECTORUPDATE_REQ;
    std::vector<int32_t> seq;
    std::vector<std::string> strseq;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        buffer.writeUint32(seq.size());
        for (const auto& item : seq) {
            buffer.writeInt32(item);
        }
        buffer.writeStringVector(strseq);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        {
            uint32_t count = reader.readUint32();
            seq.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                seq[i] = reader.readInt32();
            }
        }
        strseq = reader.readStringVector();
    }
};


struct onComplexUpdateRequest {
    uint32_t msg_id = MSG_ONCOMPLEXUPDATE_REQ;
    ComplexData data;

    void serialize(ByteBuffer& buffer) const {
        buffer.writeUint32(msg_id);
        data.serialize(buffer);
    }

    void deserialize(ByteReader& reader) {
        msg_id = reader.readUint32();
        data.deserialize(reader);
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

// Client Interface for TypeTestService
class TypeTestServiceClient : public SocketBase {
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
    TypeTestServiceClient() : listening_(false) {}

    ~TypeTestServiceClient() {
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
            case MSG_ONINTEGERUPDATE_REQ:
            case MSG_ONFLOATUPDATE_REQ:
            case MSG_ONSTRUCTUPDATE_REQ:
            case MSG_ONVECTORUPDATE_REQ:
            case MSG_ONCOMPLEXUPDATE_REQ:
                return true;
            default:
                return false;
        }
    }

    void handleBroadcastMessage(uint32_t msg_id, const uint8_t* data, size_t size) {
        ByteReader reader(data, size);
        
        switch (msg_id) {
            case MSG_ONINTEGERUPDATE_REQ: {
                onIntegerUpdateRequest request;
                request.deserialize(reader);
                onIntegerUpdate(request.i8, request.u8, request.i32, request.i64);
                break;
            }
            case MSG_ONFLOATUPDATE_REQ: {
                onFloatUpdateRequest request;
                request.deserialize(reader);
                onFloatUpdate(request.f, request.d);
                break;
            }
            case MSG_ONSTRUCTUPDATE_REQ: {
                onStructUpdateRequest request;
                request.deserialize(reader);
                onStructUpdate(request.data);
                break;
            }
            case MSG_ONVECTORUPDATE_REQ: {
                onVectorUpdateRequest request;
                request.deserialize(reader);
                onVectorUpdate(request.seq, request.strseq);
                break;
            }
            case MSG_ONCOMPLEXUPDATE_REQ: {
                onComplexUpdateRequest request;
                request.deserialize(reader);
                onComplexUpdate(request.data);
                break;
            }
            default:
                std::cout << "[Client] Received unknown broadcast message: " << msg_id << std::endl;
                break;
        }
    }

protected:
    // Callback methods (marked with 'callback' keyword in IDL)
    virtual void onIntegerUpdate(int8_t i8, uint8_t u8, int32_t i32, int64_t i64) {
        // Override to handle onIntegerUpdate callback from server
        std::cout << "[Client] 📢 Callback: onIntegerUpdate" << std::endl;
    }

    virtual void onFloatUpdate(float f, double d) {
        // Override to handle onFloatUpdate callback from server
        std::cout << "[Client] 📢 Callback: onFloatUpdate" << std::endl;
    }

    virtual void onStructUpdate(IntegerTypes data) {
        // Override to handle onStructUpdate callback from server
        std::cout << "[Client] 📢 Callback: onStructUpdate" << std::endl;
    }

    virtual void onVectorUpdate(std::vector<int32_t> seq, std::vector<std::string> strseq) {
        // Override to handle onVectorUpdate callback from server
        std::cout << "[Client] 📢 Callback: onVectorUpdate" << std::endl;
    }

    virtual void onComplexUpdate(ComplexData data) {
        // Override to handle onComplexUpdate callback from server
        std::cout << "[Client] 📢 Callback: onComplexUpdate" << std::endl;
    }

public:

    int32_t testIntegers(int8_t i8, uint8_t u8, int16_t i16, uint16_t u16, int32_t i32, uint32_t u32, int64_t i64, uint64_t u64) {
        if (!connected_) {
            return int32_t();
        }

        // Prepare request
        testIntegersRequest request;
        request.i8 = i8;
        request.u8 = u8;
        request.i16 = i16;
        request.u16 = u16;
        request.i32 = i32;
        request.u32 = u32;
        request.i64 = i64;
        request.u64 = u64;

        // Serialize and send request (thread-safe)
        std::lock_guard<std::mutex> lock(send_mutex_);
        ByteBuffer buffer;
        request.serialize(buffer);
        // Send message size first
        uint32_t msg_size = buffer.size();
        if (sendData(&msg_size, sizeof(msg_size)) < 0) {
            return int32_t();
        }
        // Send message data
        if (sendData(buffer.data(), buffer.size()) < 0) {
            return int32_t();
        }

        // Wait for response from queue (listener thread will queue it)
        uint32_t expected_msg_id = MSG_TESTINTEGERS_RESP;
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
                return int32_t(); // Timeout
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

        testIntegersResponse response;
        ByteReader reader(response_msg.data.data(), response_msg.data.size());
        response.deserialize(reader);

        return response.return_value;
    }

    double testFloats(float f, double d) {
        if (!connected_) {
            return double();
        }

        // Prepare request
        testFloatsRequest request;
        request.f = f;
        request.d = d;

        // Serialize and send request (thread-safe)
        std::lock_guard<std::mutex> lock(send_mutex_);
        ByteBuffer buffer;
        request.serialize(buffer);
        // Send message size first
        uint32_t msg_size = buffer.size();
        if (sendData(&msg_size, sizeof(msg_size)) < 0) {
            return double();
        }
        // Send message data
        if (sendData(buffer.data(), buffer.size()) < 0) {
            return double();
        }

        // Wait for response from queue (listener thread will queue it)
        uint32_t expected_msg_id = MSG_TESTFLOATS_RESP;
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
                return double(); // Timeout
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

        testFloatsResponse response;
        ByteReader reader(response_msg.data.data(), response_msg.data.size());
        response.deserialize(reader);

        return response.return_value;
    }

    bool testCharAndBool(char c, bool b) {
        if (!connected_) {
            return bool();
        }

        // Prepare request
        testCharAndBoolRequest request;
        request.c = c;
        request.b = b;

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
        uint32_t expected_msg_id = MSG_TESTCHARANDBOOL_RESP;
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

        testCharAndBoolResponse response;
        ByteReader reader(response_msg.data.data(), response_msg.data.size());
        response.deserialize(reader);

        return response.return_value;
    }

    std::string testString(const std::string& str) {
        if (!connected_) {
            return std::string();
        }

        // Prepare request
        testStringRequest request;
        request.str = str;

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
        uint32_t expected_msg_id = MSG_TESTSTRING_RESP;
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

        testStringResponse response;
        ByteReader reader(response_msg.data.data(), response_msg.data.size());
        response.deserialize(reader);

        return response.return_value;
    }

    Priority testEnum(Priority p, Status s) {
        if (!connected_) {
            return Priority();
        }

        // Prepare request
        testEnumRequest request;
        request.p = p;
        request.s = s;

        // Serialize and send request (thread-safe)
        std::lock_guard<std::mutex> lock(send_mutex_);
        ByteBuffer buffer;
        request.serialize(buffer);
        // Send message size first
        uint32_t msg_size = buffer.size();
        if (sendData(&msg_size, sizeof(msg_size)) < 0) {
            return Priority();
        }
        // Send message data
        if (sendData(buffer.data(), buffer.size()) < 0) {
            return Priority();
        }

        // Wait for response from queue (listener thread will queue it)
        uint32_t expected_msg_id = MSG_TESTENUM_RESP;
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
                return Priority(); // Timeout
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

        testEnumResponse response;
        ByteReader reader(response_msg.data.data(), response_msg.data.size());
        response.deserialize(reader);

        return response.return_value;
    }

    IntegerTypes testStruct(IntegerTypes data) {
        if (!connected_) {
            return IntegerTypes();
        }

        // Prepare request
        testStructRequest request;
        request.data = data;

        // Serialize and send request (thread-safe)
        std::lock_guard<std::mutex> lock(send_mutex_);
        ByteBuffer buffer;
        request.serialize(buffer);
        // Send message size first
        uint32_t msg_size = buffer.size();
        if (sendData(&msg_size, sizeof(msg_size)) < 0) {
            return IntegerTypes();
        }
        // Send message data
        if (sendData(buffer.data(), buffer.size()) < 0) {
            return IntegerTypes();
        }

        // Wait for response from queue (listener thread will queue it)
        uint32_t expected_msg_id = MSG_TESTSTRUCT_RESP;
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
                return IntegerTypes(); // Timeout
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

        testStructResponse response;
        ByteReader reader(response_msg.data.data(), response_msg.data.size());
        response.deserialize(reader);

        return response.return_value;
    }

    NestedData testNestedStruct(NestedData data) {
        if (!connected_) {
            return NestedData();
        }

        // Prepare request
        testNestedStructRequest request;
        request.data = data;

        // Serialize and send request (thread-safe)
        std::lock_guard<std::mutex> lock(send_mutex_);
        ByteBuffer buffer;
        request.serialize(buffer);
        // Send message size first
        uint32_t msg_size = buffer.size();
        if (sendData(&msg_size, sizeof(msg_size)) < 0) {
            return NestedData();
        }
        // Send message data
        if (sendData(buffer.data(), buffer.size()) < 0) {
            return NestedData();
        }

        // Wait for response from queue (listener thread will queue it)
        uint32_t expected_msg_id = MSG_TESTNESTEDSTRUCT_RESP;
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
                return NestedData(); // Timeout
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

        testNestedStructResponse response;
        ByteReader reader(response_msg.data.data(), response_msg.data.size());
        response.deserialize(reader);

        return response.return_value;
    }

    std::vector<int32_t> testInt32Vector(std::vector<int32_t> seq) {
        if (!connected_) {
            return std::vector<int32_t>();
        }

        // Prepare request
        testInt32VectorRequest request;
        request.seq = seq;

        // Serialize and send request (thread-safe)
        std::lock_guard<std::mutex> lock(send_mutex_);
        ByteBuffer buffer;
        request.serialize(buffer);
        // Send message size first
        uint32_t msg_size = buffer.size();
        if (sendData(&msg_size, sizeof(msg_size)) < 0) {
            return std::vector<int32_t>();
        }
        // Send message data
        if (sendData(buffer.data(), buffer.size()) < 0) {
            return std::vector<int32_t>();
        }

        // Wait for response from queue (listener thread will queue it)
        uint32_t expected_msg_id = MSG_TESTINT32VECTOR_RESP;
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
                return std::vector<int32_t>(); // Timeout
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

        testInt32VectorResponse response;
        ByteReader reader(response_msg.data.data(), response_msg.data.size());
        response.deserialize(reader);

        return response.return_value;
    }

    std::vector<uint64_t> testUInt64Vector(std::vector<uint64_t> seq) {
        if (!connected_) {
            return std::vector<uint64_t>();
        }

        // Prepare request
        testUInt64VectorRequest request;
        request.seq = seq;

        // Serialize and send request (thread-safe)
        std::lock_guard<std::mutex> lock(send_mutex_);
        ByteBuffer buffer;
        request.serialize(buffer);
        // Send message size first
        uint32_t msg_size = buffer.size();
        if (sendData(&msg_size, sizeof(msg_size)) < 0) {
            return std::vector<uint64_t>();
        }
        // Send message data
        if (sendData(buffer.data(), buffer.size()) < 0) {
            return std::vector<uint64_t>();
        }

        // Wait for response from queue (listener thread will queue it)
        uint32_t expected_msg_id = MSG_TESTUINT64VECTOR_RESP;
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
                return std::vector<uint64_t>(); // Timeout
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

        testUInt64VectorResponse response;
        ByteReader reader(response_msg.data.data(), response_msg.data.size());
        response.deserialize(reader);

        return response.return_value;
    }

    std::vector<float> testFloatVector(std::vector<float> seq) {
        if (!connected_) {
            return std::vector<float>();
        }

        // Prepare request
        testFloatVectorRequest request;
        request.seq = seq;

        // Serialize and send request (thread-safe)
        std::lock_guard<std::mutex> lock(send_mutex_);
        ByteBuffer buffer;
        request.serialize(buffer);
        // Send message size first
        uint32_t msg_size = buffer.size();
        if (sendData(&msg_size, sizeof(msg_size)) < 0) {
            return std::vector<float>();
        }
        // Send message data
        if (sendData(buffer.data(), buffer.size()) < 0) {
            return std::vector<float>();
        }

        // Wait for response from queue (listener thread will queue it)
        uint32_t expected_msg_id = MSG_TESTFLOATVECTOR_RESP;
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
                return std::vector<float>(); // Timeout
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

        testFloatVectorResponse response;
        ByteReader reader(response_msg.data.data(), response_msg.data.size());
        response.deserialize(reader);

        return response.return_value;
    }

    std::vector<double> testDoubleVector(std::vector<double> seq) {
        if (!connected_) {
            return std::vector<double>();
        }

        // Prepare request
        testDoubleVectorRequest request;
        request.seq = seq;

        // Serialize and send request (thread-safe)
        std::lock_guard<std::mutex> lock(send_mutex_);
        ByteBuffer buffer;
        request.serialize(buffer);
        // Send message size first
        uint32_t msg_size = buffer.size();
        if (sendData(&msg_size, sizeof(msg_size)) < 0) {
            return std::vector<double>();
        }
        // Send message data
        if (sendData(buffer.data(), buffer.size()) < 0) {
            return std::vector<double>();
        }

        // Wait for response from queue (listener thread will queue it)
        uint32_t expected_msg_id = MSG_TESTDOUBLEVECTOR_RESP;
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
                return std::vector<double>(); // Timeout
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

        testDoubleVectorResponse response;
        ByteReader reader(response_msg.data.data(), response_msg.data.size());
        response.deserialize(reader);

        return response.return_value;
    }

    std::vector<std::string> testStringVector(std::vector<std::string> seq) {
        if (!connected_) {
            return std::vector<std::string>();
        }

        // Prepare request
        testStringVectorRequest request;
        request.seq = seq;

        // Serialize and send request (thread-safe)
        std::lock_guard<std::mutex> lock(send_mutex_);
        ByteBuffer buffer;
        request.serialize(buffer);
        // Send message size first
        uint32_t msg_size = buffer.size();
        if (sendData(&msg_size, sizeof(msg_size)) < 0) {
            return std::vector<std::string>();
        }
        // Send message data
        if (sendData(buffer.data(), buffer.size()) < 0) {
            return std::vector<std::string>();
        }

        // Wait for response from queue (listener thread will queue it)
        uint32_t expected_msg_id = MSG_TESTSTRINGVECTOR_RESP;
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
                return std::vector<std::string>(); // Timeout
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

        testStringVectorResponse response;
        ByteReader reader(response_msg.data.data(), response_msg.data.size());
        response.deserialize(reader);

        return response.return_value;
    }

    std::vector<bool> testBoolVector(std::vector<bool> seq) {
        if (!connected_) {
            return std::vector<bool>();
        }

        // Prepare request
        testBoolVectorRequest request;
        request.seq = seq;

        // Serialize and send request (thread-safe)
        std::lock_guard<std::mutex> lock(send_mutex_);
        ByteBuffer buffer;
        request.serialize(buffer);
        // Send message size first
        uint32_t msg_size = buffer.size();
        if (sendData(&msg_size, sizeof(msg_size)) < 0) {
            return std::vector<bool>();
        }
        // Send message data
        if (sendData(buffer.data(), buffer.size()) < 0) {
            return std::vector<bool>();
        }

        // Wait for response from queue (listener thread will queue it)
        uint32_t expected_msg_id = MSG_TESTBOOLVECTOR_RESP;
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
                return std::vector<bool>(); // Timeout
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

        testBoolVectorResponse response;
        ByteReader reader(response_msg.data.data(), response_msg.data.size());
        response.deserialize(reader);

        return response.return_value;
    }

    std::vector<Priority> testEnumVector(std::vector<Priority> seq) {
        if (!connected_) {
            return std::vector<Priority>();
        }

        // Prepare request
        testEnumVectorRequest request;
        request.seq = seq;

        // Serialize and send request (thread-safe)
        std::lock_guard<std::mutex> lock(send_mutex_);
        ByteBuffer buffer;
        request.serialize(buffer);
        // Send message size first
        uint32_t msg_size = buffer.size();
        if (sendData(&msg_size, sizeof(msg_size)) < 0) {
            return std::vector<Priority>();
        }
        // Send message data
        if (sendData(buffer.data(), buffer.size()) < 0) {
            return std::vector<Priority>();
        }

        // Wait for response from queue (listener thread will queue it)
        uint32_t expected_msg_id = MSG_TESTENUMVECTOR_RESP;
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
                return std::vector<Priority>(); // Timeout
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

        testEnumVectorResponse response;
        ByteReader reader(response_msg.data.data(), response_msg.data.size());
        response.deserialize(reader);

        return response.return_value;
    }

    std::vector<IntegerTypes> testStructVector(std::vector<IntegerTypes> seq) {
        if (!connected_) {
            return std::vector<IntegerTypes>();
        }

        // Prepare request
        testStructVectorRequest request;
        request.seq = seq;

        // Serialize and send request (thread-safe)
        std::lock_guard<std::mutex> lock(send_mutex_);
        ByteBuffer buffer;
        request.serialize(buffer);
        // Send message size first
        uint32_t msg_size = buffer.size();
        if (sendData(&msg_size, sizeof(msg_size)) < 0) {
            return std::vector<IntegerTypes>();
        }
        // Send message data
        if (sendData(buffer.data(), buffer.size()) < 0) {
            return std::vector<IntegerTypes>();
        }

        // Wait for response from queue (listener thread will queue it)
        uint32_t expected_msg_id = MSG_TESTSTRUCTVECTOR_RESP;
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
                return std::vector<IntegerTypes>(); // Timeout
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

        testStructVectorResponse response;
        ByteReader reader(response_msg.data.data(), response_msg.data.size());
        response.deserialize(reader);

        return response.return_value;
    }

    std::vector<NestedData> testNestedStructVector(std::vector<NestedData> seq) {
        if (!connected_) {
            return std::vector<NestedData>();
        }

        // Prepare request
        testNestedStructVectorRequest request;
        request.seq = seq;

        // Serialize and send request (thread-safe)
        std::lock_guard<std::mutex> lock(send_mutex_);
        ByteBuffer buffer;
        request.serialize(buffer);
        // Send message size first
        uint32_t msg_size = buffer.size();
        if (sendData(&msg_size, sizeof(msg_size)) < 0) {
            return std::vector<NestedData>();
        }
        // Send message data
        if (sendData(buffer.data(), buffer.size()) < 0) {
            return std::vector<NestedData>();
        }

        // Wait for response from queue (listener thread will queue it)
        uint32_t expected_msg_id = MSG_TESTNESTEDSTRUCTVECTOR_RESP;
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
                return std::vector<NestedData>(); // Timeout
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

        testNestedStructVectorResponse response;
        ByteReader reader(response_msg.data.data(), response_msg.data.size());
        response.deserialize(reader);

        return response.return_value;
    }

    ComplexData testComplexData(ComplexData data) {
        if (!connected_) {
            return ComplexData();
        }

        // Prepare request
        testComplexDataRequest request;
        request.data = data;

        // Serialize and send request (thread-safe)
        std::lock_guard<std::mutex> lock(send_mutex_);
        ByteBuffer buffer;
        request.serialize(buffer);
        // Send message size first
        uint32_t msg_size = buffer.size();
        if (sendData(&msg_size, sizeof(msg_size)) < 0) {
            return ComplexData();
        }
        // Send message data
        if (sendData(buffer.data(), buffer.size()) < 0) {
            return ComplexData();
        }

        // Wait for response from queue (listener thread will queue it)
        uint32_t expected_msg_id = MSG_TESTCOMPLEXDATA_RESP;
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
                return ComplexData(); // Timeout
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

        testComplexDataResponse response;
        ByteReader reader(response_msg.data.data(), response_msg.data.size());
        response.deserialize(reader);

        return response.return_value;
    }

    bool testOutParams(int32_t input, int8_t& o_i8, uint8_t& o_u8, int16_t& o_i16, uint16_t& o_u16, int32_t& o_i32, uint32_t& o_u32, int64_t& o_i64, uint64_t& o_u64, float& o_f, double& o_d, char& o_c, bool& o_b, std::string& o_str, Priority& o_p) {
        if (!connected_) {
            return false;
        }

        // Prepare request
        testOutParamsRequest request;
        request.input = input;

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
        uint32_t expected_msg_id = MSG_TESTOUTPARAMS_RESP;
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

        testOutParamsResponse response;
        ByteReader reader(response_msg.data.data(), response_msg.data.size());
        response.deserialize(reader);

        o_i8 = response.o_i8;
        o_u8 = response.o_u8;
        o_i16 = response.o_i16;
        o_u16 = response.o_u16;
        o_i32 = response.o_i32;
        o_u32 = response.o_u32;
        o_i64 = response.o_i64;
        o_u64 = response.o_u64;
        o_f = response.o_f;
        o_d = response.o_d;
        o_c = response.o_c;
        o_b = response.o_b;
        o_str = response.o_str;
        o_p = response.o_p;
        return response.status == 0;
    }

    bool testOutVectors(int32_t count, std::vector<int32_t>& o_i32seq, std::vector<float>& o_fseq, std::vector<std::string>& o_strseq, std::vector<Priority>& o_pseq, std::vector<IntegerTypes>& o_structseq) {
        if (!connected_) {
            return false;
        }

        // Prepare request
        testOutVectorsRequest request;
        request.count = count;

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
        uint32_t expected_msg_id = MSG_TESTOUTVECTORS_RESP;
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

        testOutVectorsResponse response;
        ByteReader reader(response_msg.data.data(), response_msg.data.size());
        response.deserialize(reader);

        o_i32seq = response.o_i32seq;
        o_fseq = response.o_fseq;
        o_strseq = response.o_strseq;
        o_pseq = response.o_pseq;
        o_structseq = response.o_structseq;
        return response.status == 0;
    }

    bool testInOutParams(int32_t& value, std::string& str, IntegerTypes& data, std::vector<int32_t>& seq) {
        if (!connected_) {
            return false;
        }

        // Prepare request
        testInOutParamsRequest request;
        request.value = value;
        request.str = str;
        request.data = data;
        request.seq = seq;

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
        uint32_t expected_msg_id = MSG_TESTINOUTPARAMS_RESP;
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

        testInOutParamsResponse response;
        ByteReader reader(response_msg.data.data(), response_msg.data.size());
        response.deserialize(reader);

        value = response.value;
        str = response.str;
        data = response.data;
        seq = response.seq;
        return response.status == 0;
    }

};

// Server Interface for TypeTestService
class TypeTestServiceServer : public SocketBase {
private:
    int listen_fd_;
    bool running_;
    std::vector<int> client_fds_;
    mutable std::mutex clients_mutex_;
    std::vector<std::thread> client_threads_;

public:
    TypeTestServiceServer() : listen_fd_(-1), running_(false) {}

    ~TypeTestServiceServer() {
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
                case MSG_TESTINTEGERS_REQ:
                    handle_testIntegers(client_fd, msg_size);
                    break;
                case MSG_TESTFLOATS_REQ:
                    handle_testFloats(client_fd, msg_size);
                    break;
                case MSG_TESTCHARANDBOOL_REQ:
                    handle_testCharAndBool(client_fd, msg_size);
                    break;
                case MSG_TESTSTRING_REQ:
                    handle_testString(client_fd, msg_size);
                    break;
                case MSG_TESTENUM_REQ:
                    handle_testEnum(client_fd, msg_size);
                    break;
                case MSG_TESTSTRUCT_REQ:
                    handle_testStruct(client_fd, msg_size);
                    break;
                case MSG_TESTNESTEDSTRUCT_REQ:
                    handle_testNestedStruct(client_fd, msg_size);
                    break;
                case MSG_TESTINT32VECTOR_REQ:
                    handle_testInt32Vector(client_fd, msg_size);
                    break;
                case MSG_TESTUINT64VECTOR_REQ:
                    handle_testUInt64Vector(client_fd, msg_size);
                    break;
                case MSG_TESTFLOATVECTOR_REQ:
                    handle_testFloatVector(client_fd, msg_size);
                    break;
                case MSG_TESTDOUBLEVECTOR_REQ:
                    handle_testDoubleVector(client_fd, msg_size);
                    break;
                case MSG_TESTSTRINGVECTOR_REQ:
                    handle_testStringVector(client_fd, msg_size);
                    break;
                case MSG_TESTBOOLVECTOR_REQ:
                    handle_testBoolVector(client_fd, msg_size);
                    break;
                case MSG_TESTENUMVECTOR_REQ:
                    handle_testEnumVector(client_fd, msg_size);
                    break;
                case MSG_TESTSTRUCTVECTOR_REQ:
                    handle_testStructVector(client_fd, msg_size);
                    break;
                case MSG_TESTNESTEDSTRUCTVECTOR_REQ:
                    handle_testNestedStructVector(client_fd, msg_size);
                    break;
                case MSG_TESTCOMPLEXDATA_REQ:
                    handle_testComplexData(client_fd, msg_size);
                    break;
                case MSG_TESTOUTPARAMS_REQ:
                    handle_testOutParams(client_fd, msg_size);
                    break;
                case MSG_TESTOUTVECTORS_REQ:
                    handle_testOutVectors(client_fd, msg_size);
                    break;
                case MSG_TESTINOUTPARAMS_REQ:
                    handle_testInOutParams(client_fd, msg_size);
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

    void handle_testIntegers(int client_fd, uint32_t msg_size) {
        // Receive request data (size already read by handleClient)
        uint8_t recv_buffer[65536];
        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);
        if (recv_size < 0 || recv_size != msg_size) {
            return;
        }

        testIntegersRequest request;
        ByteReader reader(recv_buffer, recv_size);
        request.deserialize(reader);

        testIntegersResponse response;
        response.return_value = ontestIntegers(request.i8, request.u8, request.i16, request.u16, request.i32, request.u32, request.i64, request.u64);

        // Serialize and send response
        ByteBuffer buffer;
        response.serialize(buffer);
        // Send response size first
        uint32_t resp_size = buffer.size();
        send(client_fd, &resp_size, sizeof(resp_size), 0);
        // Send response data
        send(client_fd, buffer.data(), buffer.size(), 0);
    }

    void handle_testFloats(int client_fd, uint32_t msg_size) {
        // Receive request data (size already read by handleClient)
        uint8_t recv_buffer[65536];
        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);
        if (recv_size < 0 || recv_size != msg_size) {
            return;
        }

        testFloatsRequest request;
        ByteReader reader(recv_buffer, recv_size);
        request.deserialize(reader);

        testFloatsResponse response;
        response.return_value = ontestFloats(request.f, request.d);

        // Serialize and send response
        ByteBuffer buffer;
        response.serialize(buffer);
        // Send response size first
        uint32_t resp_size = buffer.size();
        send(client_fd, &resp_size, sizeof(resp_size), 0);
        // Send response data
        send(client_fd, buffer.data(), buffer.size(), 0);
    }

    void handle_testCharAndBool(int client_fd, uint32_t msg_size) {
        // Receive request data (size already read by handleClient)
        uint8_t recv_buffer[65536];
        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);
        if (recv_size < 0 || recv_size != msg_size) {
            return;
        }

        testCharAndBoolRequest request;
        ByteReader reader(recv_buffer, recv_size);
        request.deserialize(reader);

        testCharAndBoolResponse response;
        response.return_value = ontestCharAndBool(request.c, request.b);

        // Serialize and send response
        ByteBuffer buffer;
        response.serialize(buffer);
        // Send response size first
        uint32_t resp_size = buffer.size();
        send(client_fd, &resp_size, sizeof(resp_size), 0);
        // Send response data
        send(client_fd, buffer.data(), buffer.size(), 0);
    }

    void handle_testString(int client_fd, uint32_t msg_size) {
        // Receive request data (size already read by handleClient)
        uint8_t recv_buffer[65536];
        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);
        if (recv_size < 0 || recv_size != msg_size) {
            return;
        }

        testStringRequest request;
        ByteReader reader(recv_buffer, recv_size);
        request.deserialize(reader);

        testStringResponse response;
        response.return_value = ontestString(request.str);

        // Serialize and send response
        ByteBuffer buffer;
        response.serialize(buffer);
        // Send response size first
        uint32_t resp_size = buffer.size();
        send(client_fd, &resp_size, sizeof(resp_size), 0);
        // Send response data
        send(client_fd, buffer.data(), buffer.size(), 0);
    }

    void handle_testEnum(int client_fd, uint32_t msg_size) {
        // Receive request data (size already read by handleClient)
        uint8_t recv_buffer[65536];
        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);
        if (recv_size < 0 || recv_size != msg_size) {
            return;
        }

        testEnumRequest request;
        ByteReader reader(recv_buffer, recv_size);
        request.deserialize(reader);

        testEnumResponse response;
        response.return_value = ontestEnum(request.p, request.s);

        // Serialize and send response
        ByteBuffer buffer;
        response.serialize(buffer);
        // Send response size first
        uint32_t resp_size = buffer.size();
        send(client_fd, &resp_size, sizeof(resp_size), 0);
        // Send response data
        send(client_fd, buffer.data(), buffer.size(), 0);
    }

    void handle_testStruct(int client_fd, uint32_t msg_size) {
        // Receive request data (size already read by handleClient)
        uint8_t recv_buffer[65536];
        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);
        if (recv_size < 0 || recv_size != msg_size) {
            return;
        }

        testStructRequest request;
        ByteReader reader(recv_buffer, recv_size);
        request.deserialize(reader);

        testStructResponse response;
        response.return_value = ontestStruct(request.data);

        // Serialize and send response
        ByteBuffer buffer;
        response.serialize(buffer);
        // Send response size first
        uint32_t resp_size = buffer.size();
        send(client_fd, &resp_size, sizeof(resp_size), 0);
        // Send response data
        send(client_fd, buffer.data(), buffer.size(), 0);
    }

    void handle_testNestedStruct(int client_fd, uint32_t msg_size) {
        // Receive request data (size already read by handleClient)
        uint8_t recv_buffer[65536];
        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);
        if (recv_size < 0 || recv_size != msg_size) {
            return;
        }

        testNestedStructRequest request;
        ByteReader reader(recv_buffer, recv_size);
        request.deserialize(reader);

        testNestedStructResponse response;
        response.return_value = ontestNestedStruct(request.data);

        // Serialize and send response
        ByteBuffer buffer;
        response.serialize(buffer);
        // Send response size first
        uint32_t resp_size = buffer.size();
        send(client_fd, &resp_size, sizeof(resp_size), 0);
        // Send response data
        send(client_fd, buffer.data(), buffer.size(), 0);
    }

    void handle_testInt32Vector(int client_fd, uint32_t msg_size) {
        // Receive request data (size already read by handleClient)
        uint8_t recv_buffer[65536];
        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);
        if (recv_size < 0 || recv_size != msg_size) {
            return;
        }

        testInt32VectorRequest request;
        ByteReader reader(recv_buffer, recv_size);
        request.deserialize(reader);

        testInt32VectorResponse response;
        response.return_value = ontestInt32Vector(request.seq);

        // Serialize and send response
        ByteBuffer buffer;
        response.serialize(buffer);
        // Send response size first
        uint32_t resp_size = buffer.size();
        send(client_fd, &resp_size, sizeof(resp_size), 0);
        // Send response data
        send(client_fd, buffer.data(), buffer.size(), 0);
    }

    void handle_testUInt64Vector(int client_fd, uint32_t msg_size) {
        // Receive request data (size already read by handleClient)
        uint8_t recv_buffer[65536];
        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);
        if (recv_size < 0 || recv_size != msg_size) {
            return;
        }

        testUInt64VectorRequest request;
        ByteReader reader(recv_buffer, recv_size);
        request.deserialize(reader);

        testUInt64VectorResponse response;
        response.return_value = ontestUInt64Vector(request.seq);

        // Serialize and send response
        ByteBuffer buffer;
        response.serialize(buffer);
        // Send response size first
        uint32_t resp_size = buffer.size();
        send(client_fd, &resp_size, sizeof(resp_size), 0);
        // Send response data
        send(client_fd, buffer.data(), buffer.size(), 0);
    }

    void handle_testFloatVector(int client_fd, uint32_t msg_size) {
        // Receive request data (size already read by handleClient)
        uint8_t recv_buffer[65536];
        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);
        if (recv_size < 0 || recv_size != msg_size) {
            return;
        }

        testFloatVectorRequest request;
        ByteReader reader(recv_buffer, recv_size);
        request.deserialize(reader);

        testFloatVectorResponse response;
        response.return_value = ontestFloatVector(request.seq);

        // Serialize and send response
        ByteBuffer buffer;
        response.serialize(buffer);
        // Send response size first
        uint32_t resp_size = buffer.size();
        send(client_fd, &resp_size, sizeof(resp_size), 0);
        // Send response data
        send(client_fd, buffer.data(), buffer.size(), 0);
    }

    void handle_testDoubleVector(int client_fd, uint32_t msg_size) {
        // Receive request data (size already read by handleClient)
        uint8_t recv_buffer[65536];
        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);
        if (recv_size < 0 || recv_size != msg_size) {
            return;
        }

        testDoubleVectorRequest request;
        ByteReader reader(recv_buffer, recv_size);
        request.deserialize(reader);

        testDoubleVectorResponse response;
        response.return_value = ontestDoubleVector(request.seq);

        // Serialize and send response
        ByteBuffer buffer;
        response.serialize(buffer);
        // Send response size first
        uint32_t resp_size = buffer.size();
        send(client_fd, &resp_size, sizeof(resp_size), 0);
        // Send response data
        send(client_fd, buffer.data(), buffer.size(), 0);
    }

    void handle_testStringVector(int client_fd, uint32_t msg_size) {
        // Receive request data (size already read by handleClient)
        uint8_t recv_buffer[65536];
        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);
        if (recv_size < 0 || recv_size != msg_size) {
            return;
        }

        testStringVectorRequest request;
        ByteReader reader(recv_buffer, recv_size);
        request.deserialize(reader);

        testStringVectorResponse response;
        response.return_value = ontestStringVector(request.seq);

        // Serialize and send response
        ByteBuffer buffer;
        response.serialize(buffer);
        // Send response size first
        uint32_t resp_size = buffer.size();
        send(client_fd, &resp_size, sizeof(resp_size), 0);
        // Send response data
        send(client_fd, buffer.data(), buffer.size(), 0);
    }

    void handle_testBoolVector(int client_fd, uint32_t msg_size) {
        // Receive request data (size already read by handleClient)
        uint8_t recv_buffer[65536];
        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);
        if (recv_size < 0 || recv_size != msg_size) {
            return;
        }

        testBoolVectorRequest request;
        ByteReader reader(recv_buffer, recv_size);
        request.deserialize(reader);

        testBoolVectorResponse response;
        response.return_value = ontestBoolVector(request.seq);

        // Serialize and send response
        ByteBuffer buffer;
        response.serialize(buffer);
        // Send response size first
        uint32_t resp_size = buffer.size();
        send(client_fd, &resp_size, sizeof(resp_size), 0);
        // Send response data
        send(client_fd, buffer.data(), buffer.size(), 0);
    }

    void handle_testEnumVector(int client_fd, uint32_t msg_size) {
        // Receive request data (size already read by handleClient)
        uint8_t recv_buffer[65536];
        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);
        if (recv_size < 0 || recv_size != msg_size) {
            return;
        }

        testEnumVectorRequest request;
        ByteReader reader(recv_buffer, recv_size);
        request.deserialize(reader);

        testEnumVectorResponse response;
        response.return_value = ontestEnumVector(request.seq);

        // Serialize and send response
        ByteBuffer buffer;
        response.serialize(buffer);
        // Send response size first
        uint32_t resp_size = buffer.size();
        send(client_fd, &resp_size, sizeof(resp_size), 0);
        // Send response data
        send(client_fd, buffer.data(), buffer.size(), 0);
    }

    void handle_testStructVector(int client_fd, uint32_t msg_size) {
        // Receive request data (size already read by handleClient)
        uint8_t recv_buffer[65536];
        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);
        if (recv_size < 0 || recv_size != msg_size) {
            return;
        }

        testStructVectorRequest request;
        ByteReader reader(recv_buffer, recv_size);
        request.deserialize(reader);

        testStructVectorResponse response;
        response.return_value = ontestStructVector(request.seq);

        // Serialize and send response
        ByteBuffer buffer;
        response.serialize(buffer);
        // Send response size first
        uint32_t resp_size = buffer.size();
        send(client_fd, &resp_size, sizeof(resp_size), 0);
        // Send response data
        send(client_fd, buffer.data(), buffer.size(), 0);
    }

    void handle_testNestedStructVector(int client_fd, uint32_t msg_size) {
        // Receive request data (size already read by handleClient)
        uint8_t recv_buffer[65536];
        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);
        if (recv_size < 0 || recv_size != msg_size) {
            return;
        }

        testNestedStructVectorRequest request;
        ByteReader reader(recv_buffer, recv_size);
        request.deserialize(reader);

        testNestedStructVectorResponse response;
        response.return_value = ontestNestedStructVector(request.seq);

        // Serialize and send response
        ByteBuffer buffer;
        response.serialize(buffer);
        // Send response size first
        uint32_t resp_size = buffer.size();
        send(client_fd, &resp_size, sizeof(resp_size), 0);
        // Send response data
        send(client_fd, buffer.data(), buffer.size(), 0);
    }

    void handle_testComplexData(int client_fd, uint32_t msg_size) {
        // Receive request data (size already read by handleClient)
        uint8_t recv_buffer[65536];
        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);
        if (recv_size < 0 || recv_size != msg_size) {
            return;
        }

        testComplexDataRequest request;
        ByteReader reader(recv_buffer, recv_size);
        request.deserialize(reader);

        testComplexDataResponse response;
        response.return_value = ontestComplexData(request.data);

        // Serialize and send response
        ByteBuffer buffer;
        response.serialize(buffer);
        // Send response size first
        uint32_t resp_size = buffer.size();
        send(client_fd, &resp_size, sizeof(resp_size), 0);
        // Send response data
        send(client_fd, buffer.data(), buffer.size(), 0);
    }

    void handle_testOutParams(int client_fd, uint32_t msg_size) {
        // Receive request data (size already read by handleClient)
        uint8_t recv_buffer[65536];
        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);
        if (recv_size < 0 || recv_size != msg_size) {
            return;
        }

        testOutParamsRequest request;
        ByteReader reader(recv_buffer, recv_size);
        request.deserialize(reader);

        testOutParamsResponse response;
        ontestOutParams(request.input, response.o_i8, response.o_u8, response.o_i16, response.o_u16, response.o_i32, response.o_u32, response.o_i64, response.o_u64, response.o_f, response.o_d, response.o_c, response.o_b, response.o_str, response.o_p);

        // Serialize and send response
        ByteBuffer buffer;
        response.serialize(buffer);
        // Send response size first
        uint32_t resp_size = buffer.size();
        send(client_fd, &resp_size, sizeof(resp_size), 0);
        // Send response data
        send(client_fd, buffer.data(), buffer.size(), 0);
    }

    void handle_testOutVectors(int client_fd, uint32_t msg_size) {
        // Receive request data (size already read by handleClient)
        uint8_t recv_buffer[65536];
        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);
        if (recv_size < 0 || recv_size != msg_size) {
            return;
        }

        testOutVectorsRequest request;
        ByteReader reader(recv_buffer, recv_size);
        request.deserialize(reader);

        testOutVectorsResponse response;
        ontestOutVectors(request.count, response.o_i32seq, response.o_fseq, response.o_strseq, response.o_pseq, response.o_structseq);

        // Serialize and send response
        ByteBuffer buffer;
        response.serialize(buffer);
        // Send response size first
        uint32_t resp_size = buffer.size();
        send(client_fd, &resp_size, sizeof(resp_size), 0);
        // Send response data
        send(client_fd, buffer.data(), buffer.size(), 0);
    }

    void handle_testInOutParams(int client_fd, uint32_t msg_size) {
        // Receive request data (size already read by handleClient)
        uint8_t recv_buffer[65536];
        ssize_t recv_size = recv(client_fd, recv_buffer, msg_size, 0);
        if (recv_size < 0 || recv_size != msg_size) {
            return;
        }

        testInOutParamsRequest request;
        ByteReader reader(recv_buffer, recv_size);
        request.deserialize(reader);

        testInOutParamsResponse response;
        response.value = request.value;
        response.str = request.str;
        response.data = request.data;
        response.seq = request.seq;
        ontestInOutParams(response.value, response.str, response.data, response.seq);

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
    void push_onIntegerUpdate(int8_t i8, uint8_t u8, int32_t i32, int64_t i64, int exclude_fd = -1) {
        // Prepare callback request
        onIntegerUpdateRequest request;
        request.i8 = i8;
        request.u8 = u8;
        request.i32 = i32;
        request.i64 = i64;

        // Broadcast callback to all clients
        broadcast(request, exclude_fd);
    }

    void push_onFloatUpdate(float f, double d, int exclude_fd = -1) {
        // Prepare callback request
        onFloatUpdateRequest request;
        request.f = f;
        request.d = d;

        // Broadcast callback to all clients
        broadcast(request, exclude_fd);
    }

    void push_onStructUpdate(IntegerTypes data, int exclude_fd = -1) {
        // Prepare callback request
        onStructUpdateRequest request;
        request.data = data;

        // Broadcast callback to all clients
        broadcast(request, exclude_fd);
    }

    void push_onVectorUpdate(std::vector<int32_t> seq, std::vector<std::string> strseq, int exclude_fd = -1) {
        // Prepare callback request
        onVectorUpdateRequest request;
        request.seq = seq;
        request.strseq = strseq;

        // Broadcast callback to all clients
        broadcast(request, exclude_fd);
    }

    void push_onComplexUpdate(ComplexData data, int exclude_fd = -1) {
        // Prepare callback request
        onComplexUpdateRequest request;
        request.data = data;

        // Broadcast callback to all clients
        broadcast(request, exclude_fd);
    }

protected:
    // Virtual functions to be implemented by user
    virtual int32_t ontestIntegers(int8_t i8, uint8_t u8, int16_t i16, uint16_t u16, int32_t i32, uint32_t u32, int64_t i64, uint64_t u64) = 0;
    virtual double ontestFloats(float f, double d) = 0;
    virtual bool ontestCharAndBool(char c, bool b) = 0;
    virtual std::string ontestString(const std::string& str) = 0;
    virtual Priority ontestEnum(Priority p, Status s) = 0;
    virtual IntegerTypes ontestStruct(IntegerTypes data) = 0;
    virtual NestedData ontestNestedStruct(NestedData data) = 0;
    virtual std::vector<int32_t> ontestInt32Vector(std::vector<int32_t> seq) = 0;
    virtual std::vector<uint64_t> ontestUInt64Vector(std::vector<uint64_t> seq) = 0;
    virtual std::vector<float> ontestFloatVector(std::vector<float> seq) = 0;
    virtual std::vector<double> ontestDoubleVector(std::vector<double> seq) = 0;
    virtual std::vector<std::string> ontestStringVector(std::vector<std::string> seq) = 0;
    virtual std::vector<bool> ontestBoolVector(std::vector<bool> seq) = 0;
    virtual std::vector<Priority> ontestEnumVector(std::vector<Priority> seq) = 0;
    virtual std::vector<IntegerTypes> ontestStructVector(std::vector<IntegerTypes> seq) = 0;
    virtual std::vector<NestedData> ontestNestedStructVector(std::vector<NestedData> seq) = 0;
    virtual ComplexData ontestComplexData(ComplexData data) = 0;
    virtual void ontestOutParams(int32_t input, int8_t& o_i8, uint8_t& o_u8, int16_t& o_i16, uint16_t& o_u16, int32_t& o_i32, uint32_t& o_u32, int64_t& o_i64, uint64_t& o_u64, float& o_f, double& o_d, char& o_c, bool& o_b, std::string& o_str, Priority& o_p) = 0;
    virtual void ontestOutVectors(int32_t count, std::vector<int32_t>& o_i32seq, std::vector<float>& o_fseq, std::vector<std::string>& o_strseq, std::vector<Priority>& o_pseq, std::vector<IntegerTypes>& o_structseq) = 0;
    virtual void ontestInOutParams(int32_t& value, std::string& str, IntegerTypes& data, std::vector<int32_t>& seq) = 0;

    // Optional callbacks for client connection events
    virtual void onClientConnected(int client_fd) {
        // Override this to handle client connection
    }

    virtual void onClientDisconnected(int client_fd) {
        // Override this to handle client disconnection
    }
};

} // namespace ipc

#endif // TYPETESTSERVICE_SOCKET_HPP