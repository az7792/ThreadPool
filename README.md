### 使用方法演示
使用前需要包含ThreadPool.h
```cpp
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
    std::future<int> ans2 = t.submit([](int x, int y) -> int { return x + y; }, 2, 3);
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
```
### 测试

测试环境：Windows 11 专业版 23H2  16核 :AMD Ryzen 7 5800H with Radeon Graphics   3.20 GHz

#### 简单任务测试：

**任务：**

```c++
sum++;
```

**结果(第一行测试结果为非线程池版本)：**

|Threads|100 tasks|1000 tasks|10000 tasks|100000 tasks|1000000 tasks|10000000 tasks|
|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
|1(noPool)|0ms|0ms|0ms|1ms|14ms|133ms|
|1|0ms|0ms|15ms|190ms|1919ms|22629ms|
|2|0ms|0ms|30ms|314ms|3154ms|31944ms|
|4|0ms|0ms|19ms|235ms|2369ms|23955ms|
|8|0ms|13ms|17ms|175ms|1851ms|18402ms|
|16|10ms|3ms|19ms|192ms|1918ms|19587ms|



#### 复杂任务测试：

**任务：**

```c++
 sum++;
 int count = 0;
 for (int num = 1; num <= 1000; ++num) {
      bool isPrime = true;
      for (int i = 2; i <= sqrt(num); ++i) {
           if (num % i == 0) {
                isPrime = false;
                break;
           }
      }
      if (isPrime) ++count;
 }
 return count;
```

**结果(第一行测试结果为非线程池版本)：**

|Threads|100 tasks|1000 tasks|10000 tasks|100000 tasks|1000000 tasks|
|:---:|:---:|:---:|:---:|:---:|:---:|
1(noPool)|4ms|41ms|485ms|4864ms|49809ms|
|1|8ms|55ms|495ms|5109ms|51289ms|
|2|0ms|31ms|315ms|2970ms|29187ms|
|4|0ms|14ms|156ms|1570ms|15521ms|
|8|0ms|13ms|78ms|810ms|8037ms|
|16|0ms|0ms|48ms|462ms|4757ms|