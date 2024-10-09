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
void calculatePrimes(int start, int end)
{
    int primeCount = 0;
    for (int i = start; i <= end; ++i)
    {
        if (isPrime(i))
        {
            ++primeCount;
        }
    }
}
int main()
{
    auto start2 = std::chrono::system_clock::now();
    int n = 1e5;
    ThreadPool t(16);

    std::function<void()> task = []()
    {
        for (int i = 0; i < 10; ++i)
            sum++;
        // calculatePrimes(1, 10000);
    };

    auto start1 = std::chrono::system_clock::now();
    // for (int i = 0; i < n; ++i)
    //     task();
    auto end1 = std::chrono::system_clock::now();

    std::vector<std::future<void>> arr;

    for (int i = 0; i < n; ++i)
        t.submit(task);
    t.close();
    auto end2 = std::chrono::system_clock::now();

    // 计算时间差并转换为毫秒
    std::cout << "sum:" << sum << "\n";
    std::chrono::duration<double> diff1 = end1 - start1;
    std::chrono::duration<double> diff2 = end2 - start2;
    auto diff_in_millis1 = std::chrono::duration_cast<std::chrono::milliseconds>(diff1).count();
    auto diff_in_millis2 = std::chrono::duration_cast<std::chrono::milliseconds>(diff2).count();
    std::cout << "one thread:" << diff_in_millis1 << " ms" << std::endl;
    std::cout << "thread pool: " << diff_in_millis2 << " ms" << std::endl;
    return 0;
}
