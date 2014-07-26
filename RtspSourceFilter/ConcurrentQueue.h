#pragma once

#include <queue>
#include <condition_variable>
#include <chrono>
#include <mutex>

template <typename T>
class ConcurrentQueue
{
    std::deque<T> _queue;
    typedef std::mutex mutex_type;
    mutable mutex_type _mutex;
    std::condition_variable _condition_variable;

public:
    typedef T value_type;
    typedef T& reference_type;
    typedef const T& const_reference;

    /**
     * Construct empty queue
     */
    explicit ConcurrentQueue() {}

    /**
     * Copy constructor
     */
    ConcurrentQueue(const ConcurrentQueue& other)
    {
        std::lock_guard<mutex_type> lock(other._mutex);
        _queue = other._queue;
    }

    ConcurrentQueue& operator=(ConcurrentQueue&) = delete;

    /**
     * Enqueue an item at tail of queue.
     */
    void push(const T& data)
    {
        std::unique_lock<mutex_type> lock(_mutex);
        _queue.push_back(data);
        lock.unlock();
        _condition_variable.notify_one();
    }

    /**
     * Enqueue an item at tail of queue.
     * Overloaded version for rvalue reference
     */
    void push(T&& data)
    {
        std::unique_lock<mutex_type> lock(_mutex);
        _queue.push_back(std::forward<T>(data));
        lock.unlock();
        _condition_variable.notify_one();
    }

    /**
     * Attempt to dequeue an item from head of queue.
     * Does not wait for item to become available.
     * Returns true if successful; false otherwise.
     */
    bool try_pop(T& value)
    {
        std::lock_guard<mutex_type> lock(_mutex);
        if (_queue.empty())
            return false;
        value = std::move(_queue.front());
        _queue.pop_front();
        return true;
    }

    /**
     * Attempt to dequeue an item from head of queue.
     * Waits for item to become available for specified duration.
     * Returns true if successful; false otherwise.
     */
    bool try_pop_for(T& value, std::chrono::milliseconds duration)
    {
        std::unique_lock<mutex_type> lock(_mutex);
        bool status = _condition_variable.wait_for(lock, duration, [this]
                                                   { return !_queue.empty(); });
        if (!status || _queue.empty())
            return false;
        value = std::move(_queue.front());
        _queue.pop_front(); // calls dtor on "moved" front object
        return true;
    }

    /**
     * Dequeue item from head of queue.
     * Block until an item becomes available, and then dequeue it
     */
    void pop(T& value)
    {
        std::unique_lock<mutex_type> lock(_mutex);
        _condition_variable.wait(lock, [this]
                                 { return !_queue.empty(); });
        value = std::move(_queue.front());
        _queue.pop_front(); // calls dtor on "moved" front object
    }

    bool empty() const
    {
        std::lock_guard<mutex_type> lock(_mutex);
        return _queue.empty();
    }

    void clear()
    {
        std::lock_guard<mutex_type> lock(_mutex);
        _queue.clear();
    }

    size_t size()
    {
        std::lock_guard<mutex_type> lock(_mutex);
        return _queue.size();
    }
};
