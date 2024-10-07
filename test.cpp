//---------TEST------------
#include "ThreadPool.h"
#include <iostream>
std::atomic<int> sum = 0;
int main()
{
     ThreadPool t(16, true);
     auto start = std::chrono::system_clock::now();

     for (int i = 0; i < 100000; ++i)
     {
          t.submit([]()

                   { sum++; });
     }
     std::cout << t.getThreadNum() << "\n";
     t.close();
     std::cout << "sum:" << sum << "\n";
     auto end = std::chrono::system_clock::now();
     // 计算时间差并转换为毫秒
     std::chrono::duration<double> diff = end - start;
     auto diff_in_millis = std::chrono::duration_cast<std::chrono::milliseconds>(diff).count();
     std::cout << "Time difference: " << diff_in_millis << " ms" << std::endl;
     return 0;
}