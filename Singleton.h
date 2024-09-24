//
// Created by blair on 2024/9/4.
//

#ifndef SINGLETON_H
#define SINGLETON_H

#include <iostream>
#include <mutex>
#include <atomic>
#include <memory>


/**
 * 单例模式
 * 构造函数和析构函数是私有的，不允许外部生成和释放；
 * 静态成员变量和静态返回单例的成员函数；
 * 禁用拷贝和赋值；
 */


class Singleton1{
public:
    static std::shared_ptr<Singleton1> getInstance(){
        return _instance_ptr;
    }
    Singleton1(const Singleton1&)=delete;
    Singleton1(Singleton1&&) = delete;
    Singleton1& operator=(const Singleton1&) = delete;
    Singleton1& operator=(Singleton1&&) = delete;

private:
    Singleton1(){std::cout << "Singleton1()"<< std::endl;}    // 私有构造
    ~Singleton1(){std::cout << "~Singleton1()"<< std::endl;}
    static void destructInstance(Singleton1* ptr) {delete ptr;};
    static std::shared_ptr<Singleton1> _instance_ptr;
};

std::shared_ptr<Singleton1> Singleton1::_instance_ptr = std::shared_ptr<Singleton1>(new Singleton1(), destructInstance);


class Singleton2{
public:
    static std::shared_ptr<Singleton2> getInstance(){
        if(!_instance_ptr)
            _instance_ptr = std::shared_ptr<Singleton2>(
                new Singleton2(), [](Singleton2* ptr){delete ptr;});
        return _instance_ptr;
    }

    Singleton2(const Singleton2&)=delete;
    Singleton2(Singleton2&&) = delete;
    Singleton2& operator=(const Singleton2&) = delete;
    Singleton2& operator=(Singleton2&&) = delete;

private:
    Singleton2(){std::cout << "Singleton2()" << std::endl ;}    // 私有构造
    ~Singleton2() {std::cout << "~Singleton2()"<< std::endl;}
    static std::shared_ptr<Singleton2> _instance_ptr;
};

std::shared_ptr<Singleton2> Singleton2::_instance_ptr = nullptr;


// double-check
class Singleton3
{
public:
    static std::shared_ptr<Singleton3> getInstance()
    {
        if(_instance == nullptr)
        {
            std::lock_guard<std::mutex> lock(_mtx);
                if(!_instance)
                    _instance = std::shared_ptr<Singleton3>(
                        new Singleton3(), [](Singleton3* ptr){delete ptr;});
        }
        return _instance;
    }

    Singleton3(const Singleton3&) = delete;
    Singleton3(Singleton3 &&) = delete;
    Singleton3& operator=(const Singleton3&) = delete;
    Singleton3& operator=(Singleton3 &&) = delete;
private:

    Singleton3(){std::cout << "Singleton3()" << std::endl ;}    // 私有构造
    ~Singleton3() {std::cout << "~Singleton3()" << std::endl ;}
    static std::mutex _mtx;
    static std::shared_ptr<Singleton3> _instance;
};

std::shared_ptr<Singleton3> Singleton3::_instance = nullptr;
std::mutex Singleton3::_mtx{};


// 原子操作
class Singleton4
{
public:
    static Singleton4* getInstance()
    {
        Singleton4* tmp = _instance.load(std::memory_order_acquire);

        if(tmp == nullptr)
        {
            std::lock_guard<std::mutex> lock(_mtx);
            tmp = _instance.load(std::memory_order_relaxed);
            if(tmp == nullptr)
            {
                tmp = new Singleton4();
                _instance.store(tmp, std::memory_order_release);
                atexit(destruct);
            }
        }

        return tmp;
    }

    Singleton4(const Singleton4&) = delete;
    Singleton4(Singleton4 &&) = delete;
    Singleton4& operator=(const Singleton4&) = delete;
    Singleton4& operator=(Singleton4 &&) = delete;
private:
    static void destruct(){delete _instance;}

    Singleton4() {std::cout << "Singleton4()" << std::endl;}  // 私有构造
    ~Singleton4(){std::cout << "~Singleton4()" << std::endl;}
    static std::atomic<Singleton4*> _instance;
    static std::mutex _mtx;
};

std::atomic<Singleton4*> Singleton4::_instance = nullptr;
std::mutex Singleton4::_mtx{};

class Singleton5
{
public:
    static Singleton5* getInstance()
    {
        static Singleton5 instance{};
        return &instance;
    }

    Singleton5(const Singleton5&) = delete;
    Singleton5(Singleton5 &&) = delete;
    Singleton5& operator=(const Singleton5&) = delete;
    Singleton5& operator=(Singleton5 &&) = delete;
private:
    Singleton5(){std::cout << "Singleton5()" << std::endl;}    // 私有构造
    ~Singleton5() {std::cout << "~Singleton5()" << std::endl ;}
};


class Singleton6
{
public:
    static std::shared_ptr<Singleton6> getInstance()
    {
        std::call_once(_initFlag, []()
        {
            auto del = [](Singleton6* ptr){delete ptr;};
            _instance = std::shared_ptr<Singleton6>(new Singleton6(), del);
        });
        return _instance;
    }

    Singleton6(const Singleton6&) = delete;
    Singleton6(Singleton6 &&) = delete;
    Singleton6& operator=(const Singleton6&) = delete;
    Singleton6& operator=(Singleton6 &&) = delete;
private:
    Singleton6() {std::cout << "Singleton6()" << std::endl;}    // 私有构造
    ~Singleton6() {std::cout << "~Singleton6()" << std::endl ;}
    static std::shared_ptr<Singleton6> _instance;
    static std::once_flag _initFlag;
};

std::shared_ptr<Singleton6> Singleton6::_instance = nullptr;
std::once_flag Singleton6::_initFlag;


#endif //SINGLETON_H
