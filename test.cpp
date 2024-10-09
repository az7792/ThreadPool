//---------TEST------------
#include "ThreadPool.h"
#include <iostream>
#include <vector>
std::atomic<int> sum = 0;
// 函数：判断一个数是否是素数
bool isPrime(int n)
{
    if (n <= 1)
        return false;
    if (n == 2 || n == 3)
        return true;
    if (n % 2 == 0 || n % 3 == 0)
        return false;
    for (int i = 5; i * i <= n; i += 6)
    {
        if (n % i == 0 || n % (i + 2) == 0)
            return false;
    }
    return true;
}

// 计算密集型函数：在一个范围内计算素数
void calculatePrimes(int start, int end, int &primeCount)
{
    sum++;
    primeCount = 0;
    for (int i = start; i <= end; ++i)
    {
        if (isPrime(i))
        {
            ++primeCount;
        }
    }
}
ThreadPool t(16);
void show()
{
    while (t.isRunning())
    {
        for (auto v : t.num)
            std::cout << v << " ";
        std::cout << "end\n";
        for (auto v : t.num)
            std::cout << v << " ";
        std::cout << "A\n";
        for (auto &v : t.m_tasks)
            std::cout << v.size() << " ";
        std::cout << "B\n";
        std::cout << t.m_managerTasks.size() << "C\n";
        // for (auto &v : t.m_tasks)
        //     std::cout << v.size() << " ";
        // std::cout << "\n";
        std::cout << "runnuing:" << t.isRunning() << "\n";
        // std::cout << t.m_managerTasks.size() << "end\n";
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    for (auto v : t.num)
        std::cout << v << " ";
    std::cout << "A\n";
    for (auto &v : t.m_tasks)
        std::cout << v.size() << " ";
    std::cout << "B\n";
    std::cout << t.m_managerTasks.size() << "C\n";
    std::cout << t.isRunning() << "D\n";
}
int main()
{
    // std::thread Tt(show);
    auto start = std::chrono::system_clock::now();
    std::vector<std::future<void>> arr;
    for (int i = 0; i < 1000000; ++i)
    {
        // arr.push_back(t.submit([]()
        //                        { int ans  =0;
        //       calculatePrimes(1,10000,ans); }));
        arr.push_back(t.submit([]()
                               { sum++; }));
    }
    t.close();
    for (auto &v : arr)
        v.get();
    auto end = std::chrono::system_clock::now();
    // 计算时间差并转换为毫秒
    std::cout << "sum:" << sum << "\n";
    std::chrono::duration<double> diff = end - start;
    auto diff_in_millis = std::chrono::duration_cast<std::chrono::milliseconds>(diff).count();
    std::cout << "Time difference: " << diff_in_millis << " ms" << std::endl;

    // if (Tt.joinable())
    //     Tt.join();
    return 0;
}
