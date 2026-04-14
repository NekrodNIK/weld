#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <ctime>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <ostream>
#include <queue>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>


class ThreadPool {
private:
	struct TaskQueue {
		std::queue<std::function<void()>> tasks;
		std::mutex mutex;
		std::condition_variable cv;	
	};

	std::vector<std::unique_ptr<TaskQueue>> queues;
	std::vector<std::thread> threads;
	std::atomic<bool> running;
	std::atomic<size_t> current_worker;

	void worker(size_t thread_index) {
		assert(thread_index < threads.size());
		while (true) {
			std::function<void()> current_task;
			{
				std::unique_lock<std::mutex> lock(queues[thread_index]->mutex);
				queues[thread_index]->cv.wait(lock, [this, thread_index] () -> bool {
					return !running || !queues[thread_index]->tasks.empty();
				});
				if (!running && queues[thread_index]->tasks.empty()) {
					return;
				}
				current_task = std::move(queues[thread_index]->tasks.front());
				queues[thread_index]->tasks.pop();
			}
			current_task();
		}
	}

public:

	ThreadPool(size_t core_pool_size) : running(true), current_worker(0) {
		threads.reserve(core_pool_size);
		for (size_t i = 0; i < core_pool_size; i++) {
			queues.emplace_back(std::make_unique<TaskQueue>());
			threads.emplace_back(&ThreadPool::worker, this, i);
		}
	}

	~ThreadPool() {
		running = false;
		for (auto& q : queues) {
			q->cv.notify_one();
		}
		for (auto& t : threads) {
			t.join();
		}
	}

	template<typename Function, typename... Args>
	auto submit(Function&& func, Args&&... args) -> std::future<decltype(func(args...))> {
		using ret_type = decltype(func(args...));
		auto task = std::make_shared<std::packaged_task<ret_type()>>(std::bind(std::forward<Function>(func), 
			std::forward<Args>(args)...));
		std::future<ret_type> result = task->get_future();
		std::function<void()> wrapper = [task]() -> void { (*task)(); };
		size_t index = current_worker.fetch_add(1, std::memory_order_relaxed) % queues.size();
		{
			std::lock_guard<std::mutex> lock(queues[index]->mutex);
			queues[index]->tasks.push(std::move(wrapper));
		}
		queues[index]->cv.notify_one();
		return result;
	}

	void await_termination() {
		running = false;
		for (auto& q : queues) {
			q->cv.notify_one();
		}
		bool ready = false;
		size_t time_to_sleep = 12;
		size_t max_time = 1000;
		while (!ready) {
			ready = true;
			for (auto& q : queues) {
				{
					std::lock_guard<std::mutex> lock(q->mutex);
					if (!q->tasks.empty()) {
						ready = false;
						break;
					}
				}
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(time_to_sleep));
			time_to_sleep = std::min(time_to_sleep * 2, max_time);
		}
	}

};