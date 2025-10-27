#include <iostream>
#include <cassert>
#include "typetestservice_socket.hpp"

using namespace ipc;

// 测试结果统计
int total_tests = 0;
int passed_tests = 0;

#define TEST_START(name) \
    do { \
        std::cout << "\n[测试 " << (++total_tests) << "] " << name << " ... "; \
    } while(0)

#define TEST_PASS() \
    do { \
        std::cout << "✅ 通过" << std::endl; \
        passed_tests++; \
    } while(0)

#define TEST_FAIL(msg) \
    do { \
        std::cout << "❌ 失败: " << msg << std::endl; \
    } while(0)

int main() {
    std::cout << "=== TypeTest Client 全面测试 ===" << std::endl;
    std::cout << "连接到服务器 localhost:8888" << std::endl;
    
    TypeTestServiceClient client;
    if (!client.connect("127.0.0.1", 8888)) {
        std::cerr << "连接服务器失败" << std::endl;
        return 1;
    }
    
    std::cout << "连接成功！开始测试所有数据类型...\n" << std::endl;
    sleep(1); // 等待listener启动
    
    // ========== 测试1: 整数类型 ==========
    TEST_START("整数类型 (int8~int64, uint8~uint64)");
    try {
        int32_t result = client.testIntegers(1, 2, 3, 4, 5, 6, 7, 8);
        if (result == 1005) TEST_PASS();
        else TEST_FAIL("返回值不正确: " + std::to_string(result));
    } catch (const std::exception& e) {
        TEST_FAIL(e.what());
    }
    
    // ========== 测试2: 浮点类型 ==========
    TEST_START("浮点类型 (float, double)");
    try {
        double result = client.testFloats(3.14f, 2.718);
        if (result > 5.85 && result < 5.86) TEST_PASS();
        else TEST_FAIL("返回值不正确: " + std::to_string(result));
    } catch (const std::exception& e) {
        TEST_FAIL(e.what());
    }
    
    // ========== 测试3: 字符和布尔 ==========
    TEST_START("字符和布尔类型 (char, bool)");
    try {
        bool result = client.testCharAndBool('A', false);
        if (result == true) TEST_PASS();
        else TEST_FAIL("返回值不正确");
    } catch (const std::exception& e) {
        TEST_FAIL(e.what());
    }
    
    // ========== 测试4: 字符串 ==========
    TEST_START("字符串类型 (string)");
    try {
        std::string result = client.testString("Hello World");
        if (result == "Echo: Hello World") TEST_PASS();
        else TEST_FAIL("返回值不正确: " + result);
    } catch (const std::exception& e) {
        TEST_FAIL(e.what());
    }
    
    // ========== 测试5: 枚举类型 ==========
    TEST_START("枚举类型 (enum)");
    try {
        Priority result = client.testEnum(Priority::LOW, Status::PENDING);
        if (result == Priority::HIGH) TEST_PASS();
        else TEST_FAIL("返回值不正确");
    } catch (const std::exception& e) {
        TEST_FAIL(e.what());
    }
    
    // ========== 测试6: 结构体 ==========
    TEST_START("结构体 (struct)");
    try {
        IntegerTypes data;
        data.i8 = 1; data.u8 = 2; data.i16 = 3; data.u16 = 4;
        data.i32 = 100; data.u32 = 200; data.i64 = 1000; data.u64 = 2000;
        
        IntegerTypes result = client.testStruct(data);
        if (result.i32 == 200 && result.i64 == 2000) TEST_PASS();
        else TEST_FAIL("返回值不正确");
    } catch (const std::exception& e) {
        TEST_FAIL(e.what());
    }
    
    // ========== 测试7: 嵌套结构体 ==========
    TEST_START("嵌套结构体");
    try {
        NestedData data;
        data.integers.i32 = 50;
        data.floats.d = 1.23;
        data.priority = Priority::MEDIUM;
        data.status = Status::PROCESSING;
        
        NestedData result = client.testNestedStruct(data);
        if (result.integers.i32 == 100 && result.floats.d > 4.36) TEST_PASS();
        else TEST_FAIL("返回值不正确");
    } catch (const std::exception& e) {
        TEST_FAIL(e.what());
    }
    
    // ========== 测试8: vector<int32_t> ==========
    TEST_START("vector<int32_t>");
    try {
        std::vector<int32_t> seq = {10, 20, 30, 40, 50};
        std::vector<int32_t> result = client.testInt32Vector(seq);
        if (result.size() == 5 && result[0] == 20 && result[4] == 100) TEST_PASS();
        else TEST_FAIL("返回值不正确");
    } catch (const std::exception& e) {
        TEST_FAIL(e.what());
    }
    
    // ========== 测试9: vector<uint64_t> ==========
    TEST_START("vector<uint64_t>");
    try {
        std::vector<uint64_t> seq = {100, 200, 300};
        std::vector<uint64_t> result = client.testUInt64Vector(seq);
        if (result.size() == 3 && result[0] == 1100) TEST_PASS();
        else TEST_FAIL("返回值不正确");
    } catch (const std::exception& e) {
        TEST_FAIL(e.what());
    }
    
    // ========== 测试10: vector<float> ==========
    TEST_START("vector<float>");
    try {
        std::vector<float> seq = {1.0f, 2.0f, 3.0f};
        std::vector<float> result = client.testFloatVector(seq);
        if (result.size() == 3 && result[0] > 1.49f && result[0] < 1.51f) TEST_PASS();
        else TEST_FAIL("返回值不正确");
    } catch (const std::exception& e) {
        TEST_FAIL(e.what());
    }
    
    // ========== 测试11: vector<double> ==========
    TEST_START("vector<double>");
    try {
        std::vector<double> seq = {1.5, 2.5, 3.5};
        std::vector<double> result = client.testDoubleVector(seq);
        if (result.size() == 3 && result[0] == 3.0) TEST_PASS();
        else TEST_FAIL("返回值不正确");
    } catch (const std::exception& e) {
        TEST_FAIL(e.what());
    }
    
    // ========== 测试12: vector<string> ==========
    TEST_START("vector<string>");
    try {
        std::vector<std::string> seq = {"apple", "banana", "cherry"};
        std::vector<std::string> result = client.testStringVector(seq);
        if (result.size() == 3 && result[0] == "[apple]") TEST_PASS();
        else TEST_FAIL("返回值不正确");
    } catch (const std::exception& e) {
        TEST_FAIL(e.what());
    }
    
    // ========== 测试13: vector<bool> ==========
    TEST_START("vector<bool>");
    try {
        std::vector<bool> seq = {true, false, true};
        std::vector<bool> result = client.testBoolVector(seq);
        if (result.size() == 3 && result[0] == false && result[1] == true) TEST_PASS();
        else TEST_FAIL("返回值不正确");
    } catch (const std::exception& e) {
        TEST_FAIL(e.what());
    }
    
    // ========== 测试14: vector<enum> ==========
    TEST_START("vector<enum>");
    try {
        std::vector<Priority> seq = {Priority::LOW, Priority::HIGH, Priority::MEDIUM};
        std::vector<Priority> result = client.testEnumVector(seq);
        if (result.size() == 3) TEST_PASS();
        else TEST_FAIL("返回值不正确");
    } catch (const std::exception& e) {
        TEST_FAIL(e.what());
    }
    
    // ========== 测试15: vector<struct> ==========
    TEST_START("vector<struct>");
    try {
        std::vector<IntegerTypes> seq(3);
        for (int i = 0; i < 3; i++) {
            seq[i].i32 = i * 10;
            seq[i].i64 = i * 100;
        }
        std::vector<IntegerTypes> result = client.testStructVector(seq);
        if (result.size() == 3 && result[0].i32 == 10) TEST_PASS();
        else TEST_FAIL("返回值不正确");
    } catch (const std::exception& e) {
        TEST_FAIL(e.what());
    }
    
    // ========== 测试16: out参数 ==========
    TEST_START("out参数 (所有基础类型)");
    try {
        int8_t o_i8; uint8_t o_u8;
        int16_t o_i16; uint16_t o_u16;
        int32_t o_i32; uint32_t o_u32;
        int64_t o_i64; uint64_t o_u64;
        float o_f; double o_d;
        char o_c; bool o_b;
        std::string o_str; Priority o_p;
        
        client.testOutParams(999, o_i8, o_u8, o_i16, o_u16, o_i32, o_u32, 
                           o_i64, o_u64, o_f, o_d, o_c, o_b, o_str, o_p);
        
        if (o_i8 == -8 && o_u8 == 8 && o_i32 == -32 && o_str == "Output String") TEST_PASS();
        else TEST_FAIL("out参数值不正确");
    } catch (const std::exception& e) {
        TEST_FAIL(e.what());
    }
    
    // ========== 测试17: out vector参数 ==========
    TEST_START("out vector参数");
    try {
        std::vector<int32_t> o_i32seq;
        std::vector<float> o_fseq;
        std::vector<std::string> o_strseq;
        std::vector<Priority> o_pseq;
        std::vector<IntegerTypes> o_structseq;
        
        client.testOutVectors(5, o_i32seq, o_fseq, o_strseq, o_pseq, o_structseq);
        
        if (o_i32seq.size() == 5 && o_strseq.size() == 5 && o_structseq.size() == 5) TEST_PASS();
        else TEST_FAIL("out vector大小不正确");
    } catch (const std::exception& e) {
        TEST_FAIL(e.what());
    }
    
    // ========== 测试18: inout参数 ==========
    TEST_START("inout参数");
    try {
        int32_t value = 100;
        std::string str = "test";
        IntegerTypes data;
        data.i32 = 50;
        std::vector<int32_t> seq = {1, 2, 3};
        
        client.testInOutParams(value, str, data, seq);
        
        if (value == 200 && str == "test_modified" && data.i32 == 1049 && seq[0] == 101) TEST_PASS();
        else TEST_FAIL("inout参数值不正确");
    } catch (const std::exception& e) {
        TEST_FAIL(e.what());
    }
    
    // ========== 测试19: 复杂数据结构 ==========
    TEST_START("复杂数据结构 (包含所有vector类型)");
    try {
        ComplexData data;
        data.i32seq = {1, 2, 3};
        data.fseq = {1.1f, 2.2f};
        data.strseq = {"a", "b", "c"};
        data.priseq = {Priority::LOW, Priority::HIGH};
        
        ComplexData result = client.testComplexData(data);
        if (result.i32seq.size() == 3 && result.strseq.size() == 3) TEST_PASS();
        else TEST_FAIL("返回值不正确");
    } catch (const std::exception& e) {
        TEST_FAIL(e.what());
    }
    
    // ========== 总结 ==========
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "测试完成！" << std::endl;
    std::cout << "总测试数: " << total_tests << std::endl;
    std::cout << "通过: " << passed_tests << " ✅" << std::endl;
    std::cout << "失败: " << (total_tests - passed_tests) << " ❌" << std::endl;
    std::cout << "成功率: " << (100.0 * passed_tests / total_tests) << "%" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    std::cout << "\n测试完成，客户端退出" << std::endl;
    
    return (passed_tests == total_tests) ? 0 : 1;
}
