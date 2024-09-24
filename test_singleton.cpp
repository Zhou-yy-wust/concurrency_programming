#include "Singleton.h"


int main() {
    // 测试 Singleton1
    {
        std::cout << "Testing Singleton1:" << std::endl;
        auto s1_1 = Singleton1::getInstance();
        auto s1_2 = Singleton1::getInstance();
        std::cout << "Singleton1 instance 1 address: " << s1_1.get() << std::endl;
        std::cout << "Singleton1 instance 2 address: " << s1_2.get() << std::endl;
    }

    // 测试 Singleton2
    {
        std::cout << "\nTesting Singleton2:" << std::endl;
        auto s2_1 = Singleton2::getInstance();
        auto s2_2 = Singleton2::getInstance();
        std::cout << "Singleton2 instance 1 address: " << s2_1.get() << std::endl;
        std::cout << "Singleton2 instance 2 address: " << s2_2.get() << std::endl;
    }

    // 测试 Singleton3
    {
        std::cout << "\nTesting Singleton3:" << std::endl;
        auto s3_1 = Singleton3::getInstance();
        auto s3_2 = Singleton3::getInstance();
        std::cout << "Singleton3 instance 1 address: " << s3_1.get() << std::endl;
        std::cout << "Singleton3 instance 2 address: " << s3_2.get() << std::endl;
    }

    // 测试 Singleton4
    {
        std::cout << "\nTesting Singleton4:" << std::endl;
        Singleton4* s4_1 = Singleton4::getInstance();
        Singleton4* s4_2 = Singleton4::getInstance();
        std::cout << "Singleton4 instance 1 address: " << s4_1 << std::endl;
        std::cout << "Singleton4 instance 2 address: " << s4_2 << std::endl;
    }

    // 测试 Singleton5
    {
        std::cout << "\nTesting Singleton5:" << std::endl;
        auto s5_1 = Singleton5::getInstance();
        auto s5_2 = Singleton5::getInstance();
        std::cout << "Singleton5 instance 1 address: " << s5_1 << std::endl;
        std::cout << "Singleton5 instance 2 address: " << s5_2 << std::endl;
    }

    // 测试 Singleton6
    {
        std::cout << "\nTesting Singleton6:" << std::endl;
        auto s6_1 = Singleton6::getInstance();
        auto s6_2 = Singleton6::getInstance();
        std::cout << "Singleton6 instance 1 address: " << s6_1 << std::endl;
        std::cout << "Singleton6 instance 2 address: " << s6_2 << std::endl;
    }

    return 0;
}
