#include "typetestservice_socket.hpp"
#include <iostream>

int main() {
    TypeTestServiceClient client;
    
    std::cout << "尝试连接..." << std::endl;
    if (!client.connect("127.0.0.1", 8888)) {
        std::cout << "连接失败！" << std::endl;
        return 1;
    }
    std::cout << "连接成功！" << std::endl;
    
    // 让listener thread启动
    sleep(1);
    
    std::cout << "调用 testIntegers..." << std::endl;
    int32_t result = client.testIntegers(1, 2, 3, 4, 5, 6, 7, 8);
    std::cout << "返回值: " << result << std::endl;
    
    return 0;
}
