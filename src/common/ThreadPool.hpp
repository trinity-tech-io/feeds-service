/**
 * @file	ThreadPool.hpp
 * @brief	ThreadPool
 * @details	
 *
 * @author	xxx
 * @author	<xxx@xxx.com>
 * @copyright	(c) 2012 xxx All rights reserved.
 **/

#ifndef _ELASTOS_THREAD_POOL_HPP_
#define _ELASTOS_THREAD_POOL_HPP_

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace elastos {

class ThreadPool {
public:
    /*** type define ***/
    //using Task = std::bind<F, Args...>;
    using Task = std::function<void()>;

    /*** static function and variable ***/

    /*** class function and variable ***/
    explicit ThreadPool(const std::string& threadName, size_t threadCnt = 1);
    virtual ~ThreadPool();

    int sleepMS(long milliSecond);

    // post and copy
    void post(const Task& task);
    // post and move
    void post(Task&& task);

protected:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/
    std::string mThreadName;
    std::vector<std::thread> mThreadPool;
    std::mutex mMutex;
    std::condition_variable mCondition;
    std::queue<Task> mTaskQueue;
    bool mQuit;

    void processTaskQueue(void);

}; // class ThreadPool

} // namespace elastos

#endif /* _ELASTOS_THREAD_POOL_HPP_ */
