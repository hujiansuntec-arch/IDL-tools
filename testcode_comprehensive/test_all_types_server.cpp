#include <iostream>
#include <cstring>
#include <thread>
#include "typetestservice_socket.hpp"

using namespace ipc;

class TypeTestServiceImpl : public TypeTestServiceServer {
public:
    // 测试整数类型
    int32_t ontestIntegers(int8_t i8, uint8_t u8, int16_t i16, uint16_t u16,
                           int32_t i32, uint32_t u32, int64_t i64, uint64_t u64) override {
        std::cout << "testIntegers: i8=" << (int)i8 << " u8=" << (int)u8 
                  << " i16=" << i16 << " u16=" << u16
                  << " i32=" << i32 << " u32=" << u32 
                  << " i64=" << i64 << " u64=" << u64 << std::endl;
        return i32 + 1000;
    }
    
    // 测试浮点类型
    double ontestFloats(float f, double d) override {
        std::cout << "testFloats: f=" << f << " d=" << d << std::endl;
        return f + d;
    }
    
    // 测试字符和布尔
    bool ontestCharAndBool(char c, bool b) override {
        std::cout << "testCharAndBool: c='" << c << "' b=" << b << std::endl;
        return !b;
    }
    
    // 测试string
    std::string ontestString(const std::string& str) override {
        std::cout << "testString: str=\"" << str << "\"" << std::endl;
        return "Echo: " + str;
    }
    
    // 测试enum
    Priority ontestEnum(Priority p, Status s) override {
        std::cout << "testEnum: p=" << (int)p << " s=" << (int)s << std::endl;
        return Priority::HIGH;
    }
    
    // 测试struct
    IntegerTypes ontestStruct(IntegerTypes data) override {
        std::cout << "testStruct: i32=" << data.i32 << " i64=" << data.i64 << std::endl;
        data.i32 += 100;
        data.i64 += 1000;
        return data;
    }
    
    // 测试嵌套struct
    NestedData ontestNestedStruct(NestedData data) override {
        std::cout << "testNestedStruct: integers.i32=" << data.integers.i32 
                  << " floats.d=" << data.floats.d << std::endl;
        data.integers.i32 += 50;
        data.floats.d += 3.14;
        return data;
    }
    
    // 测试vector<int32_t>
    std::vector<int32_t> ontestInt32Vector(std::vector<int32_t> seq) override {
        std::cout << "testInt32Vector: size=" << seq.size() << " [";
        for (size_t i = 0; i < seq.size() && i < 5; i++) {
            std::cout << seq[i] << " ";
        }
        std::cout << "]" << std::endl;
        
        std::vector<int32_t> result;
        for (auto v : seq) result.push_back(v * 2);
        return result;
    }
    
    // 测试vector<uint64_t>
    std::vector<uint64_t> ontestUInt64Vector(std::vector<uint64_t> seq) override {
        std::cout << "testUInt64Vector: size=" << seq.size() << std::endl;
        std::vector<uint64_t> result;
        for (auto v : seq) result.push_back(v + 1000);
        return result;
    }
    
    // 测试vector<float>
    std::vector<float> ontestFloatVector(std::vector<float> seq) override {
        std::cout << "testFloatVector: size=" << seq.size() << std::endl;
        std::vector<float> result;
        for (auto v : seq) result.push_back(v * 1.5f);
        return result;
    }
    
    // 测试vector<double>
    std::vector<double> ontestDoubleVector(std::vector<double> seq) override {
        std::cout << "testDoubleVector: size=" << seq.size() << std::endl;
        std::vector<double> result;
        for (auto v : seq) result.push_back(v * 2.0);
        return result;
    }
    
    // 测试vector<string>
    std::vector<std::string> ontestStringVector(std::vector<std::string> seq) override {
        std::cout << "testStringVector: size=" << seq.size() << std::endl;
        std::vector<std::string> result;
        for (auto& s : seq) result.push_back("[" + s + "]");
        return result;
    }
    
    // 测试vector<bool>
    std::vector<bool> ontestBoolVector(std::vector<bool> seq) override {
        std::cout << "testBoolVector: size=" << seq.size() << std::endl;
        std::vector<bool> result;
        for (auto b : seq) result.push_back(!b);
        return result;
    }
    
    // 测试vector<enum>
    std::vector<Priority> ontestEnumVector(std::vector<Priority> seq) override {
        std::cout << "testEnumVector: size=" << seq.size() << std::endl;
        return seq;
    }
    
    // 测试vector<struct>
    std::vector<IntegerTypes> ontestStructVector(std::vector<IntegerTypes> seq) override {
        std::cout << "testStructVector: size=" << seq.size() << std::endl;
        for (auto& item : seq) {
            item.i32 += 10;
        }
        return seq;
    }
    
    // 测试vector<嵌套struct>
    std::vector<NestedData> ontestNestedStructVector(std::vector<NestedData> seq) override {
        std::cout << "testNestedStructVector: size=" << seq.size() << std::endl;
        return seq;
    }
    
    // 测试复杂数据
    ComplexData ontestComplexData(ComplexData data) override {
        std::cout << "testComplexData: i32seq.size=" << data.i32seq.size() 
                  << " strseq.size=" << data.strseq.size() << std::endl;
        return data;
    }
    
    // 测试out参数
    void ontestOutParams(int32_t input, 
                        int8_t& o_i8, uint8_t& o_u8,
                        int16_t& o_i16, uint16_t& o_u16,
                        int32_t& o_i32, uint32_t& o_u32,
                        int64_t& o_i64, uint64_t& o_u64,
                        float& o_f, double& o_d,
                        char& o_c, bool& o_b,
                        std::string& o_str, Priority& o_p) override {
        std::cout << "testOutParams: input=" << input << std::endl;
        o_i8 = -8; o_u8 = 8;
        o_i16 = -16; o_u16 = 16;
        o_i32 = -32; o_u32 = 32;
        o_i64 = -64; o_u64 = 64;
        o_f = 3.14f; o_d = 2.718;
        o_c = 'X'; o_b = true;
        o_str = "Output String";
        o_p = Priority::CRITICAL;
    }
    
    // 测试out vector参数
    void ontestOutVectors(int32_t count,
                         std::vector<int32_t>& o_i32seq,
                         std::vector<float>& o_fseq,
                         std::vector<std::string>& o_strseq,
                         std::vector<Priority>& o_pseq,
                         std::vector<IntegerTypes>& o_structseq) override {
        std::cout << "testOutVectors: count=" << count << std::endl;
        
        for (int i = 0; i < count; i++) {
            o_i32seq.push_back(i * 10);
            o_fseq.push_back(i * 1.5f);
            o_strseq.push_back("str_" + std::to_string(i));
            o_pseq.push_back(i % 2 ? Priority::HIGH : Priority::LOW);
            
            IntegerTypes it;
            it.i32 = i; it.i64 = i * 100;
            o_structseq.push_back(it);
        }
    }
    
    // 测试inout参数
    void ontestInOutParams(int32_t& value, std::string& str, 
                          IntegerTypes& data, std::vector<int32_t>& seq) override {
        std::cout << "testInOutParams: value=" << value << " str=\"" << str 
                  << "\" data.i32=" << data.i32 << " seq.size=" << seq.size() << std::endl;
        value *= 2;
        str = str + "_modified";
        data.i32 += 999;
        for (auto& v : seq) v += 100;
    }
    
    // 不需要实现callback方法，服务器端使用push_xxx方法推送
};

int main() {
    std::cout << "=== TypeTest Server 启动 ===" << std::endl;
    std::cout << "监听端口: 8888" << std::endl;
    
    TypeTestServiceImpl server;
    if (!server.start(8888)) {
        std::cerr << "启动服务器失败" << std::endl;
        return 1;
    }
    
    std::cout << "服务器运行中，按Ctrl+C退出..." << std::endl;
    
    // 启动回调推送线程
    std::thread callback_thread([&server]() {
        int counter = 0;
        while (true) {
            sleep(10);
            counter++;
            
            std::cout << "\n推送回调 #" << counter << std::endl;
            server.push_onIntegerUpdate(1, 2, counter, counter * 100);
            server.push_onFloatUpdate(3.14f * counter, 2.718 * counter);
            
            IntegerTypes it;
            it.i8 = counter; it.i32 = counter * 10; it.i64 = counter * 100;
            server.push_onStructUpdate(it);
            
            std::vector<int32_t> seq = {counter, counter*2, counter*3};
            std::vector<std::string> strseq = {"push1", "push2"};
            server.push_onVectorUpdate(seq, strseq);
        }
    });
    
    // 运行服务器主循环（接受连接）
    server.run();
    
    return 0;
}
