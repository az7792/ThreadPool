#pragma once
#include <mutex>
#include <deque>
template <typename T>
class SafeQueue
{
public:
    SafeQueue() = default;

    // 添加元素到队列
    void push(const T &value)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.emplace_back(value);
    }

    void push(T &&value)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.emplace_back(std::move(value));
    }

    // 弹出元素
    void pop()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.pop_front();
    }

    void pop(T &value)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        value = std::move(m_queue.front());
        m_queue.pop_front();
    }

    // 获取队头元素
    T &front()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.front();
    }

    // 获取队列大小
    size_t size()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.size();
    }

    // 检查队列是否为空
    bool empty()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }

private:
    std::deque<T> m_queue;
    std::mutex m_mutex;
};