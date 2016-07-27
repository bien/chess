#include "mtqueue.hh"


Queue::Queue(int size)
	: data(size), start_position(0), end_position(0), max_size(size), current_size(0), shutting_down(false)
{	
}
	
void Queue::push(const T& item)
{
	std::lock_guard<std::mutex> lock(mutex);
	while (current_size >= max_size) {
		full.wait(lock);
	}
	end_position = (end_position + 1) % max_size;
	++current_size;
	data[end_position] = item;
	empty.notify_one();
}

bool Queue::pop(T &item)
{
	std::lock_guard<std::mutex> lock(mutex);
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

void Queue::shutdown()
{
	while (current_size > 0) {
		empty.wait(lock);
	}
	
	shutting_down = true;
	empty.notify_all();
}
