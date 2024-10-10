#include "ThreadPool.h"
#include <iostream>
#include <future>
#include <memory>
#include <functional>

int add(int x, int y)
{
    return x + y;
}

class A
{
public:
    int operator()(int x, int y) { return x + y; }
};

class B
{
public:
    int add(int x, int y) { return x + y; }
    static int staticAdd(int x, int y) { return x + y; }
};

int main()
{
    ThreadPool t(0); // 线程数，0为系统物理核心
    /* 使用 submit 提交可执行对象，返回值在 std::future<T> 中
     * 在需要返回值的时候使用future的get方法获取
     * get方法会阻塞当前线程，所以尽量不要提前获取返回值
     */

    // 普通函数
    std::future<int> ans1 = t.submit(add, 1, 2);
    std::cout << "普通函数: " << ans1.get() << std::endl;

    // lambda表达式
    std::future<int> ans2 = t.submit([](int x, int y) -> int
                                     { return x + y; }, 2, 3);
    std::cout << "lambda表达式: " << ans2.get() << std::endl;

    // 仿函数
    A a;
    std::future<int> ans3 = t.submit(a, 3, 4);
    std::cout << "仿函数: " << ans3.get() << std::endl;

    // 类成员函数
    B b;
    std::future<int> ans4 = t.submit(&B::add, b, 4, 5);
    std::cout << "类成员函数: " << ans4.get() << std::endl;

    // 静态成员函数
    std::future<int> ans5 = t.submit(&B::staticAdd, 5, 6);
    std::cout << "静态成员函数: " << ans5.get() << std::endl;

    // 函数指针
    int (*addPtr)(int, int) = add;
    std::future<int> ans6 = t.submit(addPtr, 6, 7);
    std::cout << "函数指针: " << ans6.get() << std::endl;

    // 使用智能指针
    std::shared_ptr<B> b_ptr = std::make_shared<B>();
    std::future<int> ans7 = t.submit(&B::add, b_ptr, 7, 8);
    std::cout << "智能指针调用成员函数: " << ans7.get() << std::endl;

    // 使用 std::bind 绑定对象的成员函数
    auto boundFunc = std::bind(&B::add, b, std::placeholders::_1, std::placeholders::_2);
    std::future<int> ans8 = t.submit(boundFunc, 8, 9);
    std::cout << "std::bind绑定成员函数: " << ans8.get() << std::endl;

    //......
    return 0;
}