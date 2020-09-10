//
//  ThreadPool.cpp
//
//  Created by mengxk on 19/03/16.
//  Copyright © 2016 mengxk. All rights reserved.
//

#include "ThreadPool.hpp"

#include "Log.hpp"
#include "Platform.hpp"

namespace elastos {

/***********************************************/
/***** static variables initialize *************/
/***********************************************/


/***********************************************/
/***** static function implement ***************/
/***********************************************/


/***********************************************/
/***** class public function implement  ********/
/***********************************************/
ThreadPool::ThreadPool(const std::string& threadName, size_t threadCnt)
    : mThreadName(threadName)
    , mThreadPool(threadCnt)
    , mMutex()
    , mCondition()
    , mTaskQueue()
    , mQuit(false)
{
    Log::D(Log::TAG, "%s name:%s count:%d", FORMAT_METHOD, threadName.c_str(), threadCnt);

	for(size_t idx = 0; idx < mThreadPool.size(); idx++) {
		mThreadPool[idx] = std::thread(&ThreadPool::processTaskQueue, this);
	}
}

ThreadPool::~ThreadPool()
{
	std::unique_lock<std::mutex> lock(mMutex);
	mQuit = true;
    auto empty = std::queue<Task>();
    std::swap(mTaskQueue, empty); // mTaskQueue.clear();
	lock.unlock();
	mCondition.notify_all();

	// Wait for threads to finish before we exit
	for(size_t idx = 0; idx < mThreadPool.size(); idx++) {
		auto& it = mThreadPool[idx];
		if(it.joinable()) {
            Log::D(Log::TAG, "%s Joining thread %d until completion. tid=%d:%d",
            		         FORMAT_METHOD, idx, it.get_id(), std::this_thread::get_id());
			it.join();
			Log::D(Log::TAG, "%s Joined thread.", FORMAT_METHOD);
		}
	}
    mThreadPool.clear();

    Log::D(Log::TAG, "%s name:%s", FORMAT_METHOD, mThreadName.c_str());
}

int ThreadPool::sleepMS(long milliSecond)
{
	auto interval = 100; // ms
	auto elapsed = 0;
    do {
		auto remains = milliSecond - elapsed;
		auto needsleep = interval < remains  ? interval : remains;

		std::this_thread::sleep_for(std::chrono::milliseconds(needsleep));
		elapsed += needsleep;
		if(mQuit == true) {
			return -1;
		}

	} while (elapsed < milliSecond);

	return 0;
}

void ThreadPool::post(const Task& task)
{
	if(mQuit == true) {
		return;
	}

	std::unique_lock<std::mutex> lock(mMutex);
	mTaskQueue.push(task);

	// Manual unlocking is done before notifying, to avoid waking up
    // the waiting thread only to block again (see notify_one for details)
	lock.unlock();
	mCondition.notify_all();
}

void ThreadPool::post(Task&& task)
{
	if(mQuit == true) {
		return;
	}

	std::unique_lock<std::mutex> lock(mMutex);
	mTaskQueue.push(std::move(task));

	// Manual unlocking is done before notifying, to avoid waking up
    // the waiting thread only to block again (see notify_one for details)
	lock.unlock();
	mCondition.notify_all();
}

/***********************************************/
/***** class protected function implement  *****/
/***********************************************/


/***********************************************/
/***** class private function implement  *******/
/***********************************************/
void ThreadPool::processTaskQueue(void)
{
	std::unique_lock<std::mutex> lock(mMutex);

	do {
		//Wait until we have data or a quit signal
		mCondition.wait(lock, [this]{
			return (mTaskQueue.size() || mQuit);
		});

		//after wait, we own the lock
		if(!mQuit && mTaskQueue.size())
		{
			auto task = std::move(mTaskQueue.front());
			mTaskQueue.pop();

			//unlock now that we're done messing with the queue
			lock.unlock();

			task();

			lock.lock();
		}
	} while (!mQuit);

//	Platform::DetachCurrentThread();
	Log::D(Log::TAG, "%s name:%s exit.", FORMAT_METHOD, mThreadName.c_str());
}

} // namespace elastos
