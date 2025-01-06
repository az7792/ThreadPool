#pragma once
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <vector>
#include <functional>
#include <future>
#include <atomic>
/**
 * @class ThreadPool
 * @brief 线程池实现，提交的任务会先进入任务分配器的任务队列，再由任务分配器分发给工作线程
 * 提交的任务的返回值存放在std::future<> 中，在需要时使用future的get()方法获取返回值
 */
class ThreadPool
{
     // 线程池运行状态
     std::atomic<bool> m_running;

     // 每个线程单独一条队列，单独一个条件变量 ，单独一把锁
     int threadCount;                                             // 线程数
     std::vector<std::thread> m_workers;                          // 工作线程
     std::vector<std::unique_ptr<std::mutex>> m_mutexs;           // 锁m_runnings[index]，*m_tasks[index]， *m_CVs[index]
     std::vector<bool> m_runnings;                                // 线程是否运行
     std::vector<std::unique_ptr<std::condition_variable>> m_CVs; // 各个线程的条件变量
     std::vector<std::deque<std::function<void()>>> m_tasks;      // 各个线程的任务队列

     // 最大最小线程数
     const int m_maxThreads = std::max(128, 2 * static_cast<int>(std::thread::hardware_concurrency()));
     const int m_minThreads = 1;

public:
     ThreadPool(const ThreadPool &other) = delete;            // 禁止拷贝构造
     ThreadPool(ThreadPool &&other) = delete;                 // 禁止移动构造
     ThreadPool &operator=(const ThreadPool &other) = delete; // 禁止拷贝赋值
     ThreadPool &operator=(ThreadPool &&other) = delete;      // 禁止移动赋值

     /**
      * @brief  初始化线程池
      * threadCount会被限制在[m_minThreads,m_maxThreads]范围内
      * @param threadCount  线程池大小，0表示使用物理核心数
      */
     explicit ThreadPool(int threadCt)
         : m_running(true)
     {
          // 调整线程数到合适范围
          threadCount = threadCt == 0 ? std::thread::hardware_concurrency() : threadCt;
          if (threadCount < m_minThreads)
               threadCount = m_minThreads;
          else if (threadCount > m_maxThreads)
               threadCount = m_maxThreads;

          m_runnings.resize(threadCount, true), m_tasks.resize(threadCount);
          for (int i = 0; i < threadCount; ++i) // threadCount在构造函数中已经被限定
          {
               // mutex , condition_variable 不可复制不可移动
               m_mutexs.emplace_back(std::make_unique<std::mutex>());
               m_CVs.emplace_back(std::make_unique<std::condition_variable>());
          }

          // 不能合并到上面的循环，否则会导致锁和条件变量未初始化完成，线程运行可能导致错误
          for (int i = 0; i < threadCount; ++i)
               m_workers.emplace_back(&ThreadPool::worker, this, i);
     }

     ~ThreadPool() { close(); }

     /**
      * @brief 关闭线程池
      *
      * 设置线程池运行状态为false，先关闭工作线程，此时任务分发器仍能继续分配任务
      * 此时任务队列被禁止添加新的任务，但仍需要完成任务队列中剩余的任务才能完全退出
      * @warning 如果添加了阻塞的任务(例如loop循环)，请确保该任务已经提前退出，否则可能导致线程池无法正常退出
      * @warning 关闭后无法启用
      */
     void close()
     {
          if (!m_running)
               return;
          m_running = false;

          // 通知工作线程准备退出
          for (int i = 0; i < threadCount; ++i)
          {
               {
                    std::unique_lock<std::mutex> lock(*m_mutexs[i]);
                    m_runnings[i] = false;
               }
               (*m_CVs[i]).notify_one();
               if (m_workers[i].joinable())
                    m_workers[i].join();
          }
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
      *  3. 通知任务分发器分发任务
      */
     template <typename F, typename... Args>
     auto submit(F &&f, Args &&...args) -> std::future<typename std::invoke_result<F, Args...>::type>
     {
          using returnType = typename std::invoke_result<F, Args...>::type;

          auto task = std::make_shared<std::packaged_task<returnType()>>(
              std::bind(std::forward<F>(f), std::forward<Args>(args)...));
          {
               if (!m_running)
                    throw std::runtime_error("线程池已停止,无法添加新任务!");

               // 分配任务
               manager([task]()
                       { (*task)(); });
          }
          return task->get_future();
     }

     inline bool isRunning()
     {
          return m_running;
     }

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
          while (true)
          {
               // wait会先判断谓词是否满足
               std::unique_lock<std::mutex> lock(*m_mutexs[index]);
               if (!m_tasks[index].empty())
               {
                    task = std::move(m_tasks[index].front());
                    m_tasks[index].pop_front();
                    lock.unlock();

                    task();
                    continue;
               }
               else
               {
                    lock.unlock();
                    // 尝试去其他任务队列拿任务
                    // 可以拿自己的任务
                    // TODO 优化锁的开销，使用无锁队列，或者放弃窃取机制
                    for (int i = 0; i < threadCount; ++i)
                    {
                         std::unique_lock<std::mutex> otherLock(*m_mutexs[i]);
                         if (m_tasks[i].empty())
                              continue;
                         task = std::move(m_tasks[i].back());
                         m_tasks[i].pop_back();
                         otherLock.unlock();
                         task();
                         break;
                    }
               }
               lock.lock();
               // 等待直到 有任务可用 | 线程需要停止
               (*m_CVs[index]).wait(lock, [this, index]()
                                    { return !m_tasks[index].empty() || !m_runnings[index]; });
               if (!m_runnings[index] && m_tasks[index].empty()) // 没任务且线程需要停止时退出
                    return;
          }
     }

     /**
      * @brief 任务分发器，负责将任务分配给各个线程的任务队列
      */
     void manager(std::function<void()> task)
     {
          static int nowIndex = 0, nextIndex;

          // 分配任务
          if (threadCount == 1) // 只有一个线程时直接分配任务
          {
               std::unique_lock<std::mutex> nowLock(*m_mutexs[nowIndex]);
               m_tasks[nowIndex].emplace_back(std::move(task));
               (*m_CVs[nowIndex]).notify_one();
               return;
          }

          // 获取下一个线程的索引 nextIndex = (nowIndex + 1) % threadCount;
          nextIndex = nowIndex + 1;
          if (nextIndex == threadCount)
               nextIndex = 0;

          // 在当前队列与下一个队列之间优先选择任务数较小的那个队列
          {
               std::unique_lock<std::mutex> nowLock(*m_mutexs[nowIndex]);
               std::unique_lock<std::mutex> nextLock(*m_mutexs[nextIndex]);
               if (m_tasks[nowIndex].size() <= m_tasks[nextIndex].size())
               {
                    nextLock.unlock();
                    m_tasks[nowIndex].emplace_back(std::move(task));
                    (*m_CVs[nowIndex]).notify_one();
               }
               else
               {
                    nowLock.unlock();
                    m_tasks[nextIndex].emplace_back(std::move(task));
                    (*m_CVs[nextIndex]).notify_one();
               }
          }
          nowIndex = nextIndex;
     }
};