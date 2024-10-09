#include "../ThreadPool.h"
#include <chrono>
#include <map>
using namespace std;
atomic<int> sum = 0;
function<void()> task = []()
{ sum++; };
int main()
{
     // 测试是否有任务丢失
     vector<int> testVal;
     vector<int> realVal;

     // 不同线程数不同任务数用时
     vector<int64_t> unuseThreadPool;
     map<int, map<int, int64_t>> useThreadPool;

     for (int j = 1e2; j <= 1e7; j *= 10) // 任务数量
     {
          realVal.push_back(j), sum = 0;
          auto now = std::chrono::system_clock::now(); // 开始时间
          for (int k = 0; k < j; ++k)
               task();
          auto end = std::chrono::system_clock::now();                                                    // 结束时间
          auto diff_in_millis = std::chrono::duration_cast<std::chrono::milliseconds>(end - now).count(); // 实际用时(ms)
          unuseThreadPool.push_back(diff_in_millis);
          testVal.push_back(sum); // 实际任务执行次数
     }

     for (int i = 1; i <= 16; i *= 2) // 线程数
     {

          for (int j = 1e2; j <= 1e7; j *= 10)
          {
               ThreadPool t(i);
               realVal.push_back(j), sum = 0;

               auto now = std::chrono::system_clock::now();
               for (int k = 0; k < j; ++k)
                    t.submit([]()
                             { task(); });
               t.close();
               auto end = std::chrono::system_clock::now();
               auto diff_in_millis = std::chrono::duration_cast<std::chrono::milliseconds>(end - now).count();
               useThreadPool[i][j] = diff_in_millis;
               testVal.push_back(sum);
          }
     }
     if (testVal != realVal)
     {
          std::cout << "run error!\n";
          return -1;
     }

     //--------输出MD表格---------
     cout << "|Threads";
     for (auto &[x, y] : (*useThreadPool.begin()).second)
          cout << "|" << x << " tasks";
     cout << "|\n";
     for (int i = 0; i <= (*useThreadPool.begin()).second.size(); ++i)
          cout << "|:---:";
     cout << "|\n";

     cout << "1(noPool)";
     for (auto v : unuseThreadPool)
          cout << "|" << v << "ms";
     cout << "|\n";

     for (auto &[x, y] : useThreadPool)
     {
          cout << "|" << x;
          for (auto &[xx, yy] : y)
               cout << "|" << yy << "ms";
          cout << "|\n";
     }
     return 0;
}