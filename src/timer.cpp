

#include "timer.hpp"
#include "log.hpp"

Timer::Timer()
	: m_running(false)
{

}

Timer::~Timer()
{
	stop();
}

void Timer::add(int interval, timeout handler)
{

}

/// ������ʱ��
void Timer::start()
{
	m_running = true;
	m_timerThread.reset(new std::thread([this]()
		{
			loop();
		}));
}

/// ֹͣ��ʱ��
void Timer::stop()
{
	m_running = false;
	if (m_timerThread)
	{
		m_timerThread->join();
		m_timerThread = nullptr;
	}
}

void Timer::loop()
{
	while (m_running)
	{
		LOG_DEBUG("tick");
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
}