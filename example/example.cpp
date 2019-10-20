#include <thread>
#include <chrono>

#include "../src/collector.h"

#define sleep(x) std::this_thread::sleep_for(std::chrono::seconds(x))
#define COLLECTOR_SIZE 30

void print_data_callback(uint8_t* buf, size_t size)
{
	fwrite(buf, sizeof(uint8_t), size, stdout);
}

int main()
{
	Collector collector(COLLECTOR_SIZE, print_data_callback);
	collector.start();

	int thread_stop = 0;
	std::thread thread1(
		[&]()
		{
			std::string str("First thread\n");
			while (!thread_stop)
			{
				collector.push(str);
				sleep(1);
			}
		});

	std::thread thread2(
		[&]()
		{
			std::string str("Second thread\n");
			while (!thread_stop)
			{
				collector.push(str);
				sleep(1);
			}
		});

	sleep(10);
	thread_stop = 1;
	thread1.join();
	thread2.join();

	return 0;
}
