#ifndef QUEUE_HH_
#define QUEUE_HH_

#include <algorithm>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <thread>

template <typename T>
class Queue {
public:
	Queue(int size);
	void push(const T& item);
	bool pop(T &item);
	
	void shutdown(); // waits for queue to be emptied
	
private:
	std::vector<T> data;
	int start_position;
	int end_position;

	int max_size;
	int current_size;
	std::mutex mutex;
	std::condition_variable full;
	std::condition_variable empty;
	
	bool shutting_down;
};

template <typename T>
class ThreadPool {
public:
	ThreadPool(int numthreads, Queue<T> *queue, std::function<void(T)> &handler);
	~ThreadPool();

private:
	void worker_thread();
	std::vector<std::thread> threads;
	Queue<T> *queue;
	std::function<void(T)> *handler;
};

template <typename T>
Queue<T>::Queue(int size)
	: data(size), start_position(0), end_position(0), max_size(size), current_size(0), shutting_down(false)
{	
}
	
template <typename T>
void Queue<T>::push(const T& item)
{
	std::unique_lock<std::mutex> lock(mutex);
	while (current_size >= max_size) {
		full.wait(lock);
	}
	end_position = (end_position + 1) % max_size;
	++current_size;
	data[end_position] = item;
	empty.notify_one();
}

template <typename T>
bool Queue<T>::pop(T &item)
{
	std::unique_lock<std::mutex> lock(mutex);
	while (current_size == 0) {
		empty.wait(lock);
		if (shutting_down) {
			return false;
		}
	}
	--current_size;
	item = data[start_position];
	start_position = (start_position + 1) % max_size;
	full.notify_one();
	return true;
}

template <typename T>
void Queue<T>::shutdown()
{
	std::unique_lock<std::mutex> lock(mutex);
	while (current_size > 0) {
		empty.wait(mutex);
	}
	
	shutting_down = true;
	empty.notify_all();
}

template <typename T>
void ThreadPool<T>::worker_thread()
{
	T item;
	while (queue->pop(item)) {
		(*handler)(item);
	}
}

template <typename T>
ThreadPool<T>::ThreadPool(int numthreads, Queue<T> *queue, std::function<void(T)> &handler)
	: queue(queue), handler(&handler)
{
	for (int i = 0; i < numthreads; i++) {
		std::thread thread(&ThreadPool::worker_thread, this);
		threads.push_back(thread);
	}
}

template <typename T>
ThreadPool<T>::~ThreadPool()
{
	queue->shutdown();
	for (auto thread = threads.begin(); thread != threads.end(); thread++) {
		thread->join();
	}
}


#endif
