#include "Sleep.h"
#include <chrono>
#include <thread>

namespace SleepUtil {
	void sleep (unsigned long secs)
    {
        std::this_thread::sleep_for(std::chrono::seconds(secs));
	}


	void msleep (unsigned long msecs)
	{
        std::this_thread::sleep_for(std::chrono::milliseconds(msecs));
	}


	void usleep (unsigned long usecs)
	{
        std::this_thread::sleep_for(std::chrono::microseconds(usecs));
    }
}
