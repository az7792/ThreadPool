#pragma once
#include <mutex>
#include <deque>
#include <optional>

template <typename T>
class SafeQueue
{
public:
    SafeQueue() = default;

    /**
     * @brief 将元素添加到队列
     *
     * @param Args 参数，使用方式与 std::deque 一致
     * T 类型的左值     会采用拷贝构造
     * T 类型的右值     会采用移动构造
     * T 类型的构造参数 会采用原地构造
     */
    template <typename... Args>
    void emplace_front(Args &&...args)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.emplace_front(std::forward<Args>(args)...);
    }
    template <typename... Args>
    void emplace_back(Args &&...args)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.emplace_back(std::forward<Args>(args)...);
    }

    // 弹出元素，pop前需要手动判空
    void pop_front()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.pop_front();
    }
    void pop_back()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.pop_back();
    }

    // try pop并返回pop结果
    bool try_pop_front(T &value)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_queue.empty())
            return false;
        value = std::move(m_queue.front());
        m_queue.pop_front();
        return true;
    }
    bool try_pop_back(T &value)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_queue.empty())
            return false;
        value = std::move(m_queue.back());
        m_queue.pop_back();
        return true;
    }

    T &front()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.front();
    }
    T &back()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.back();
    }

    std::size_t size()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.size();
    }

    bool empty()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }

    void clear()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.clear();
    }

private:
    std::deque<T> m_queue;
    std::mutex m_mutex;
};