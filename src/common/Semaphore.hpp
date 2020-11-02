#ifndef _ELASTOS_SEMAPHORE_HPP_
#define _ELASTOS_SEMAPHORE_HPP_

#include <mutex>
#include <condition_variable>

namespace elastos {

class Semaphore
{
private:
    std::mutex mutex_;
    std::condition_variable condition_;
    unsigned long count_ = 0; // Initialized as locked.

public:
    void notify() {
        std::lock_guard<decltype(mutex_)> lock(mutex_);
        ++count_;
        condition_.notify_one();
    }

    void wait() {
        std::unique_lock<decltype(mutex_)> lock(mutex_);
        while(!count_) // Handle spurious wake-ups.
            condition_.wait(lock);
        --count_;
    }

    bool waitfor(int timeout) {
        std::unique_lock<decltype(mutex_)> lock(mutex_);
        while(!count_) {// Handle spurious wake-ups.
            auto ret = condition_.wait_for(lock, std::chrono::milliseconds(timeout));
            if(ret == std::cv_status::timeout) {
                return false;
            }
        }
        --count_;

        return true;
    }

    bool try_wait() {
        std::lock_guard<decltype(mutex_)> lock(mutex_);
        if(count_) {
            --count_;
            return true;
        }
        return false;
    }
};

} // namespace elastos

#endif /* _ELASTOS_SEMAPHORE_HPP_ */