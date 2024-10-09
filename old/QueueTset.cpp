#include "SafeQueue.h"
#include <iostream>
class A
{
public:
     int m_tmp = 0;
     A() { std::cout << "默认构造\n"; }
     A(int tmp) : m_tmp(tmp) { std::cout << "有参构造1\n"; }
     A(int tmp, int temp) : m_tmp(tmp + temp) { std::cout << "有参构造2\n"; }
     A(const A &other)
     {
          this->m_tmp = other.m_tmp;
          std::cout << "拷贝构造\n";
     }
     A(A &&other) noexcept
     {
          this->m_tmp = other.m_tmp;
          other.m_tmp = 0; // 模拟转移内存操作权限
          std::cout << "移动构造\n";
     }
     A &operator=(const A &other)
     {
          this->m_tmp = other.m_tmp;
          std::cout << "拷贝赋值\n";
          return *this;
     }
     A &operator=(A &&other) noexcept
     {
          this->m_tmp = other.m_tmp;
          other.m_tmp = 0; // 模拟转移内存操作权限
          std::cout << "移动赋值\n";
          return *this;
     }
     ~A() { std::cout << "析构\n"; }
};

#include <future>
#include <functional>
SafeQueue<std::function<void()>> m_managerTasks;
template <typename F, typename... Args>
auto submit(F &&f, Args &&...args) -> std::future<typename std::invoke_result<F, Args...>::type>
{
     using returnType = typename std::invoke_result<F, Args...>::type;

     auto task = std::make_shared<std::packaged_task<returnType()>>(
         std::bind(std::forward<F>(f), std::forward<Args>(args)...));

     m_managerTasks.emplace_back([task]()
                                 { (*task)(); });

     return task->get_future();
}

int main()
{
     A a(1);
     A &&b = std::move(a);
     //auto c = std::move(b);
     std::cout << a.m_tmp;
     std::cout << b.m_tmp;
     //std::cout << c.m_tmp;

     // SafeQueue<A> q;
     // q.emplace_back(1);
     // q.emplace_back(A(2));
     // A aa = A(3);
     // q.emplace_back(std::move(aa));
     // while (!q.empty())
     // {
     //      std::cout << q.front().m_tmp << "\n";
     //      q.pop_front();
     // }

     // // SafeQueue<std::deque<int>> Q;
     // // for (int i = 0; i < 10; ++i)
     // // {
     // //      Q.emplace_back(std::deque<int>());
     // //      for (int j = 0; j < 10; ++j)
     // //      {
     // //           Q.back().emplace_back(j);
     // //      }
     // // }
     // // for (int i = 0; i < 10; ++i)
     // // {
     // //      Q.emplace_back(std::deque<int>());
     // //      for (int j = 0; j < 10; ++j)
     // //      {
     // //           std::cout << Q.front().front() << " ";
     // //           Q.front().pop_front();
     // //      }
     // //      Q.pop_front();
     // //      std::cout << "\n";
     // // }
     // // system("pause");
     // return 0;
}