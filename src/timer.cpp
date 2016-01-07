

#include "timer.hpp"
#include "log.hpp"

Timer::Timer()
	: m_running(false)
{

}

Timer::~Timer()
{
	stop();
	m_taskList.clear();
}

void Timer::add(int interval, timeout handler)
{
	std::lock_guard<std::recursive_mutex> lock(m_taskListLock);
	m_taskList.emplace_back(interval, handler);
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
		try {
			m_timerThread->join();
		}
		catch (int errno)
		{
			LOG_DEBUG("thread join error:" << errno);
		}
		m_timerThread = nullptr;
	}
}

void Timer::loop()
{
	while (m_running)
	{		
		std::this_thread::sleep_for(std::chrono::seconds(1));

		std::lock_guard<std::recursive_mutex> lock(m_taskListLock);

		for (auto t = m_taskList.begin(); t != m_taskList.end(); )
		{
			// ����֪ͨ��ɾ��������
			if (--t->interval <= 0 && (t->interval = t->handler()) <= 0)
			{
				t = m_taskList.erase(t);
			}
			else
			{
				t++;
			}
		}
	}
}