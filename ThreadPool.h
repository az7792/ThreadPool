#pragma once
/**
 * 基于C++17
 * @class ThreadPool
 * @brief 线程池实现，支持动态调整线程数量
 * 提交的任务的返回值存放在std::future<> 中，使用future的get()方法获取返回值
 *
 */
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <deque>
#include <vector>
#include <map>
#include <functional>
#include <future>
#include "SafeQueue.h"
class ThreadPool
{
public:
     std::vector<int> num;
     // 线程池运行状态
     bool m_running;

     // 任务分发器及任务队列
     std::thread m_manager;
     std::mutex m_managerMutex; // 锁 m_running，m_managerTasks， m_managerCV
     std::condition_variable m_managerCV;
     SafeQueue<std::function<void()>> m_managerTasks;

     // 每个线程单独一条队列，单独一个条件变量
     std::vector<std::thread> m_workers;
     std::vector<std::mutex> m_mutexs; // 锁m_runnings[index]，m_tasks[index]， m_CVs[index]
     std::vector<bool> m_runnings;
     std::vector<std::condition_variable> m_CVs;
     std::vector<SafeQueue<std::function<void()>>> m_tasks;

     // 最大最小线程数
     std::atomic<unsigned int> m_maxThreads = std::max(128u, 2 * std::thread::hardware_concurrency());
     std::atomic<unsigned int> m_minThreads = 1u;

public:
     ThreadPool(const ThreadPool &other) = delete;            // 禁止拷贝构造
     ThreadPool(ThreadPool &&other) = delete;                 // 禁止移动构造
     ThreadPool &operator=(const ThreadPool &other) = delete; // 禁止拷贝赋值
     ThreadPool &operator=(ThreadPool &&other) = delete;      // 禁止移动赋值

     /**
      * @brief  初始化线程池
      *
      * threadCount会被限制在[m_minThreads,m_maxThreads]范围内
      *
      * @param threadCount  线程池大小，0表示使用物理核心数
      */
     explicit ThreadPool(size_t threadCount)
         : m_running(true), m_mutexs(threadCount), m_runnings(threadCount, true),
           m_CVs(threadCount), m_tasks(threadCount)
     {
          // 调整线程数到合适范围
          threadCount = threadCount == 0 ? std::thread::hardware_concurrency() : threadCount;
          if (threadCount < m_minThreads)
               threadCount = m_minThreads;
          else if (threadCount > m_maxThreads)
               threadCount = m_maxThreads;
          num.resize(16, 0);
          addWorkers(threadCount);

          m_manager = std::thread([this]()
                                  { manager(); });
     }

     ~ThreadPool()
     {
          close();
     }

     /**
      * @brief 关闭线程池
      *
      * 设置线程池运行状态为false，先关闭工作线程，此时任务分发器仍能继续分配任务
      * 此时任务队列被禁止添加新的任务，但仍需要完成任务队列中剩余的任务才能完全退出
      */
     void close()
     {
          {
               std::lock_guard<std::mutex> lock(m_managerMutex);
               if (!m_running)
                    return;
               m_running = false;
          }

          // 分配完全部任务后会退出
          m_managerCV.notify_one();
          if (m_manager.joinable())
               m_manager.join();
     }

     /**
      * @brief 将任务添加到任务队列，并返回该任务的 future 以便获取结果
      *
      * 该函数接收一个可调用对象（如函数、lambda、函数对象等）和可选的参数，并将它们打包为一个任务
      * 任务将被添加到任务队列中，任务分发器会将任务分配给各个线程自己的任务队列
      * 调用者可以通过返回的 std::future 对象获取任务的执行结果
      *
      * @tparam F 函数类型，表示可调用对象（如函数、lambda等）
      * @tparam Args 参数类型，可变参数模板，用于传递给可调用对象
      *
      * @param f 可调用对象，即将要执行的任务
      * @param args 可选的参数，将传递给可调用对象 f
      *
      * @return std::future<typename std::invoke_result<F, Args...>::type>
      *         返回一个 future 对象，通过它可以获取任务的执行结果
      *         如果任务返回 void，future 将表示任务完成状态
      *
      * @throws std::runtime_error 如果线程池已停止运行，无法提交新任务时抛出异常
      *
      * @details
      *  1. 使用 std::packaged_task 来包装传入的任务函数和参数，使其返回值可以通过 future 获取
      *  2. 任务被转换为一个可调用对象（lambda）并被添加到任务队列 m_tasks 中
      *  3. 唤醒一个工作线程执行任务，并通知管理者线程检查是否需要调整线程池中的工作线程数量
      */
     template <typename F, typename... Args>
     auto submit(F &&f, Args &&...args) -> std::future<typename std::invoke_result<F, Args...>::type>
     {
          using returnType = typename std::invoke_result<F, Args...>::type;

          auto task = std::make_shared<std::packaged_task<returnType()>>(
              std::bind(std::forward<F>(f), std::forward<Args>(args)...));
          {
               std::lock_guard<std::mutex> lock(m_managerMutex);
               if (!m_running)
                    throw std::runtime_error("ThreadPool is stopped, cannot submit new task.");

               m_managerTasks.emplace_back([task]()
                                           { (*task)(); });
               m_managerCV.notify_one(); // 通知管理者线程有新任务
          }
          return task->get_future();
     }

     inline bool isRunning() { return m_running; }

private:
     /**
      * @brief 线程的工作流程函数
      *
      * 首先尝试获取自己任务队列的任务，获得了就执行
      * 没获得就尝试去其他线程的任务队列取任务，随后尝试wait
      */
     void worker(int index)
     {
          std::function<void()> task;
          bool ok = false;
          while (true) // true确保线程池停止后以及任务分配器停止后仍能继续处理剩余的任务
          {
               {
                    std::lock_guard<std::mutex> lock(m_mutexs[index]);
                    ok = m_tasks[index].try_pop_front(task);
               }
               if (ok)
               {
                    task();
                    num[index]++;
               }
               else
               {
                    // 尝试去其他任务队列拿任务
                    // for (int i = 0; i < (int)m_workers.size(); ++i) // 可以拿自己的任务
                    // {
                    //      task = m_tasks[i].try_pop_back();
                    //      if (task != std::nullopt)
                    //      {
                    //           task.value()();
                    //           break;
                    //      }
                    // }

                    // 窃取不到新任务 | 窃取的任务执行完 后wait，wait会先判断谓词是否满足
                    std::unique_lock<std::mutex> lock(m_mutexs[index]);
                    // 等待直到 有任务可用 | 线程池停止
                    m_CVs[index].wait(lock, [this, index]()
                                      { return !m_tasks[index].empty() || !m_runnings[index]; });

                    // 确保任务分配器退出后保证不会再有新的任务，防止退出后仍被分配任务
                    if (!m_runnings[index] && m_tasks[index].empty())
                         return;
               }
          }
     }

     /**
      * @brief 增加指定个数的线程
      */
     void addWorkers(size_t threadCount)
     {
          m_workers.reserve(threadCount);          // 预先分配空间，并没有实际创建对象
          for (size_t i = 0; i < threadCount; ++i) // threadCount在构造函数中已经被限定
          {
               m_workers.emplace_back([this, i]()
                                      { worker(i); });
          }
     }

     /**
      * @brief 管理者线程的工作流程函数
      *
      * 该函数用于分配任务到各个工作线程的任务队列种
      */
     void manager()
     {
          int nowIndex = 0, nextIndex, maxIndex = m_workers.size();
          std::function<void()> task;
          bool ok;
          /* 防止任务分配完但线程池还在运行时退出
           * 防止线程池停止但任务队列还有任务时退出
           */
          while (true)
          {

               {
                    std::lock_guard<std::mutex> lock(m_managerMutex);
                    ok = m_managerTasks.try_pop_front(task);
               }
               if (ok)
               {

                    nextIndex = nowIndex + 1;
                    if (nextIndex == maxIndex)
                         nextIndex = 0;
                    // 在当前队列与下一个队列之间优先选择任务数较小的那个队列
                    {
                         std::unique_lock<std::mutex> nowLock(m_mutexs[nowIndex]);
                         std::unique_lock<std::mutex> nextLock(m_mutexs[nextIndex]);
                         if (m_tasks[nowIndex].size() <= m_tasks[nextIndex].size())
                         {
                              nextLock.unlock();
                              m_tasks[nowIndex].emplace_back(std::move(task));
                              m_CVs[nowIndex].notify_one();
                         }
                         else
                         {
                              nowLock.unlock();
                              m_tasks[nextIndex].emplace_back(std::move(task));
                              m_CVs[nextIndex].notify_one();
                         }
                    }
                    nowIndex = nextIndex;
               }
               else
               {
                    std::unique_lock<std::mutex> lock(m_managerMutex);
                    m_managerCV.wait(lock, [this]()
                                     { return !m_managerTasks.empty() || !m_running; });
                    if (!m_running && m_managerTasks.empty())
                         break;
               }
          }
          for (int i = 0; i < maxIndex; ++i)
          {
               {
                    std::unique_lock<std::mutex> lock(m_mutexs[i]);
                    m_runnings[i] = false;
                    m_CVs[i].notify_one();
               }
               if (m_workers[i].joinable())
                    m_workers[i].join();
          }
     }
};