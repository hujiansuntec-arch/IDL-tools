// Example server implementation for TypeTestService
#include "typetestservice_socket.hpp"
#include <iostream>

using namespace ipc;

class MyTypeTestServiceServer : public ipc::TypeTestServiceServer {
protected:
    int32_t ontestIntegers(int8_t i8, uint8_t u8, int16_t i16, uint16_t u16, int32_t i32, uint32_t u32, int64_t i64, uint64_t u64) override {
        // TODO: Implement testIntegers
        std::cout << "testIntegers called" << std::endl;
        return int32_t();
    }

    double ontestFloats(float f, double d) override {
        // TODO: Implement testFloats
        std::cout << "testFloats called" << std::endl;
        return double();
    }

    bool ontestCharAndBool(char c, bool b) override {
        // TODO: Implement testCharAndBool
        std::cout << "testCharAndBool called" << std::endl;
        return true;
    }

    std::string ontestString(const std::string& str) override {
        // TODO: Implement testString
        std::cout << "testString called" << std::endl;
        return "result";
    }

    Priority ontestEnum(Priority p, Status s) override {
        // TODO: Implement testEnum
        std::cout << "testEnum called" << std::endl;
        return Priority();
    }

    IntegerTypes ontestStruct(IntegerTypes data) override {
        // TODO: Implement testStruct
        std::cout << "testStruct called" << std::endl;
        return IntegerTypes();
    }

    NestedData ontestNestedStruct(NestedData data) override {
        // TODO: Implement testNestedStruct
        std::cout << "testNestedStruct called" << std::endl;
        return NestedData();
    }

    std::vector<int32_t> ontestInt32Vector(std::vector<int32_t> seq) override {
        // TODO: Implement testInt32Vector
        std::cout << "testInt32Vector called" << std::endl;
        return std::vector<int32_t>();
    }

    std::vector<uint64_t> ontestUInt64Vector(std::vector<uint64_t> seq) override {
        // TODO: Implement testUInt64Vector
        std::cout << "testUInt64Vector called" << std::endl;
        return std::vector<uint64_t>();
    }

    std::vector<float> ontestFloatVector(std::vector<float> seq) override {
        // TODO: Implement testFloatVector
        std::cout << "testFloatVector called" << std::endl;
        return std::vector<float>();
    }

    std::vector<double> ontestDoubleVector(std::vector<double> seq) override {
        // TODO: Implement testDoubleVector
        std::cout << "testDoubleVector called" << std::endl;
        return std::vector<double>();
    }

    std::vector<std::string> ontestStringVector(std::vector<std::string> seq) override {
        // TODO: Implement testStringVector
        std::cout << "testStringVector called" << std::endl;
        return std::vector<std::string>();
    }

    std::vector<bool> ontestBoolVector(std::vector<bool> seq) override {
        // TODO: Implement testBoolVector
        std::cout << "testBoolVector called" << std::endl;
        return std::vector<bool>();
    }

    std::vector<Priority> ontestEnumVector(std::vector<Priority> seq) override {
        // TODO: Implement testEnumVector
        std::cout << "testEnumVector called" << std::endl;
        return std::vector<Priority>();
    }

    std::vector<IntegerTypes> ontestStructVector(std::vector<IntegerTypes> seq) override {
        // TODO: Implement testStructVector
        std::cout << "testStructVector called" << std::endl;
        return std::vector<IntegerTypes>();
    }

    std::vector<NestedData> ontestNestedStructVector(std::vector<NestedData> seq) override {
        // TODO: Implement testNestedStructVector
        std::cout << "testNestedStructVector called" << std::endl;
        return std::vector<NestedData>();
    }

    ComplexData ontestComplexData(ComplexData data) override {
        // TODO: Implement testComplexData
        std::cout << "testComplexData called" << std::endl;
        return ComplexData();
    }

    void ontestOutParams(int32_t input, int8_t& o_i8, uint8_t& o_u8, int16_t& o_i16, uint16_t& o_u16, int32_t& o_i32, uint32_t& o_u32, int64_t& o_i64, uint64_t& o_u64, float& o_f, double& o_d, char& o_c, bool& o_b, std::string& o_str, Priority& o_p) override {
        // TODO: Implement testOutParams
        std::cout << "testOutParams called" << std::endl;
    }

    void ontestOutVectors(int32_t count, std::vector<int32_t>& o_i32seq, std::vector<float>& o_fseq, std::vector<std::string>& o_strseq, std::vector<Priority>& o_pseq, std::vector<IntegerTypes>& o_structseq) override {
        // TODO: Implement testOutVectors
        std::cout << "testOutVectors called" << std::endl;
    }

    void ontestInOutParams(int32_t& value, std::string& str, IntegerTypes& data, std::vector<int32_t>& seq) override {
        // TODO: Implement testInOutParams
        std::cout << "testInOutParams called" << std::endl;
    }

};

int main() {
    MyTypeTestServiceServer server;

    if (!server.start(8888)) {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }

    std::cout << "Server started on port 8888" << std::endl;
    server.run();

    return 0;
}