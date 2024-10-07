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
    // 任务队列
    SafeQueue<std::function<void()>> m_tasks;

    // 线程池运行状态
    std::atomic<bool> m_running;

    // 常驻线程
    std::vector<std::thread> m_workers;

    // 临时线程
    std::map<std::thread::id, std::thread> m_tempWorkers = {};

    // 线程退出队列
    SafeQueue<std::thread> m_threadExitQueue;

    // 常驻线程的最大最小值，临时线程的最大值
    std::atomic<unsigned int> m_maxThreads = std::max(20u, 2 * std::thread::hardware_concurrency());
    std::atomic<unsigned int> m_minThreads = 2u;

    // 管理者线程,
    std::thread m_manager;
    std::mutex m_managerMutex;
    std::condition_variable m_managerCV;

    // 公共锁
    std::mutex m_mutex;
    std::condition_variable m_cv;

    // 临时线程 m_tempWorkers 的锁
    std::mutex m_tempMutex;

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
     * @param managerRunning  是否启用线程池大小动态调整，默认为false，表示不启用
     */
    explicit ThreadPool(size_t threadCount, bool managerRunning = false)
        : m_running(true)
    {
        threadCount = threadCount == 0 ? std::thread::hardware_concurrency() : threadCount;
        if (threadCount < m_minThreads)
            threadCount = m_minThreads;
        else if (threadCount > m_maxThreads)
            threadCount = m_maxThreads;

        addWorkers(threadCount, false);

        if (managerRunning)
            m_manager = std::thread([this]()
                                    { manager(); });
    }

    ~ThreadPool()
    {
        if (m_running)
            close();
    }

    /**
     * @brief 关闭线程池
     *
     * 设置线程池运行状态为false，并依次关闭所有的线程
     * 此时任务队列被禁止添加新的任务，但仍需要完成任务队列中剩余的任务才能完全退出
     */
    void close()
    {
        m_running = false;

        m_managerCV.notify_one();
        if (m_manager.joinable())
            m_manager.join();

        while (!m_threadExitQueue.empty())
        {
            std::thread t;
            m_threadExitQueue.pop(t);
            if (t.joinable())
                t.join();
        }

        m_cv.notify_all();
        for (auto &[id, worker] : m_tempWorkers)
            if (worker.joinable())
                worker.join();
        for (auto &worker : m_workers)
            if (worker.joinable())
                worker.join();
    }

    /**
     * @brief 将任务添加到任务队列，并返回该任务的 future 以便获取结果
     *
     * 该函数接收一个可调用对象（如函数、lambda、函数对象等）和可选的参数，并将它们打包为一个任务
     * 任务将被添加到任务队列中，线程池中的工作线程随后会从队列中获取并执行任务
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

        if (!m_running.load())
            throw std::runtime_error("ThreadPool is stopped, cannot submit new task.");

        m_tasks.push([task]()
                     { (*task)(); });

        m_cv.notify_one();        // 通知一个线程取任务
        m_managerCV.notify_one(); // 通知管理者线程有新任务
        return task->get_future();
    }

    inline bool isRunning() { return m_running; }

    inline int getThreadNum()
    {
        std::lock_guard<std::mutex> lock(m_tempMutex);
        return m_workers.size() + m_tempWorkers.size();
    }

private:
    /**
     * @brief 常驻线程的工作流程函数
     *
     * 常驻线程会在任务队列中不断获取任务并执行，直到线程池被停止为止
     */
    void worker()
    {
        while (true) // true确保线程池停止后仍能继续处理剩余的任务
        {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                // 等待直到 有任务可用 | 线程池停止
                m_cv.wait(lock, [this]()
                          { return !m_tasks.empty() || !m_running; });

                if (!m_running && m_tasks.empty()) // 如果线程池停止且没有任务，退出
                    return;
                m_tasks.pop(task);
            }
            try
            {
                task();
            }
            catch (const std::exception &e)
            {
                // 处理标准异常
                std::cerr << "Task threw an exception: " << e.what() << std::endl;
            }
            catch (...)
            {
                // 处理其他异常
                std::cerr << "Task threw an unknown exception." << std::endl;
            }
        }
    }

    /**
     * @brief 临时线程的工作流程函数
     *
     * 临时线程会在任务队列中不断获取任务并执行，直到线程池被停止为止或超时进入退出队列
     */
    void tempWorker()
    {
        while (true) // true确保线程池停止后仍能继续处理剩余的任务
        {
            // 设置最大等待时间为 5 秒
            auto waitTime = std::chrono::seconds(5);
            auto now = std::chrono::system_clock::now();
            auto timeout = now + waitTime;

            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                // 等待直到 有任务可用 | 线程池停止 | 超时(超时返回false)
                bool wasNotified = m_cv.wait_until(lock, timeout, [this]()
                                                   { return !m_tasks.empty() || !m_running; });

                if (!wasNotified) // 超时未被通知
                {
                    std::lock_guard<std::mutex> tempLock(m_tempMutex);
                    m_threadExitQueue.push(std::move(m_tempWorkers[std::this_thread::get_id()]));
                    m_tempWorkers.erase(std::this_thread::get_id());
                    return;
                }

                if (!m_running && m_tasks.empty()) // 如果线程池停止且没有任务，退出
                    return;
                m_tasks.pop(task);
            }
            try
            {
                task();
            }
            catch (const std::exception &e)
            {
                // 处理标准异常
                std::cerr << "Task threw an exception: " << e.what() << std::endl;
            }
            catch (...)
            {
                // 处理其他异常
                std::cerr << "Task threw an unknown exception." << std::endl;
            }
        }
    }

    /**
     * @brief 增加指定个数的线程
     *
     * 添加的线程数不会超过m_maxThreads
     */
    bool addWorkers(size_t threadCount, bool isTemp = false)
    {
        int addNum = 0;
        if (isTemp) // 增加临时线程
        {
            std::lock_guard<std::mutex> tempLock(m_tempMutex);
            for (size_t i = 0; i < threadCount && m_tempWorkers.size() < m_maxThreads; ++i)
            {
                std::thread t([this]()
                              { tempWorker(); });
                m_tempWorkers.emplace(t.get_id(), std::move(t));
                ++addNum;
            }
        }
        else // 增加常驻线程
        {
            m_workers.reserve(threadCount);
            for (size_t i = 0; i < threadCount; ++i) // threadCount在构造函数中已经被限定
            {
                std::thread t([this]()
                              { worker(); });
                m_workers.emplace_back(std::move(t));
                ++addNum;
            }
        }
        return addNum > 0;
    }

    /**
     * @brief 管理者线程的工作流程函数
     *
     * 该函数用于动态调整线程池的工作线程数量，并清理退出的线程。
     *
     * 主要功能包括：
     * 1. 动态扩容：当任务队列的长度达到常驻线程数量的两倍时，添加临时线程以缓解任务压力。
     * 2. 清理线程：负责清理已经超时且需要退出的临时线程，确保线程池保持合理的资源占用。
     *
     * 线程扩容机制：
     * - 扩容操作会在以下两种情况下触发：
     *   1. 任务队列中的任务数量大于或等于常驻线程数量的两倍（即任务量过大，常驻线程处理不过来时）。
     *   2. 距离上一次扩容操作超过1秒，防止频繁扩容。
     *
     * 清理机制：
     * - 在每次超时等待后，检查是否有需要退出的临时线程。
     * - 如果临时线程超时没有新的任务，则将它们移入退出队列，并清理已完成任务的线程。
     *
     * 停止机制：
     * - 当线程池关闭时，管理者线程退出，停止扩容和线程清理操作。
     */
    void manager()
    {
        auto lastScaleUp = std::chrono::system_clock::now();
        while (m_running)
        {
            std::unique_lock<std::mutex> lock(m_managerMutex);
            // 设置最大等待时间为 2 秒
            auto waitTime = std::chrono::seconds(2);
            auto now = std::chrono::system_clock::now();
            auto timeout = now + waitTime;

            // 距离上次扩容超过1秒 | 任务队列长度为线程池大小的两倍 | 线程池停止 | 超时
            bool wasNotified = m_managerCV.wait_until(lock, timeout, [this, &lastScaleUp]()
                                                      {
                                                      auto now = std::chrono::system_clock::now();
                                                      return (std::chrono::duration_cast<std::chrono::seconds>(now - lastScaleUp) >= std::chrono::seconds(1) && 
                                                              m_tasks.size() >= 2 * m_workers.size()) || 
                                                             !m_running; });
            if (!m_running)
                return;

            while (!m_threadExitQueue.empty())
            {
                std::thread t;
                m_threadExitQueue.pop(t);
                if (t.joinable())
                    t.join();
            }

            // 扩容出m_workers.size()数量的临时线程
            if (m_tasks.size() >= 2 * m_workers.size())
            {
                bool ok = addWorkers(m_workers.size(), true);
                if (ok)                                             // 有效扩容
                    lastScaleUp = std::chrono::system_clock::now(); // 更新上一次扩容的时间
            }
        }
    }
};