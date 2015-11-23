
#ifndef _SIGNAL_HANDLER_H
#define _SIGNAL_HANDLER_H

//#include "config.hpp"
#include "log.hpp"
#include <asio/signal_set.hpp>
#include <signal.h>
#include <mutex>
#include <condition_variable>

/*
signal list in windows:
	SIGABRT,SIGFPE,SIGILL,SIGINT,SIGSEGV,SIGTERM
 */

namespace ics {

class SignalHandler {
public:
	SignalHandler(asio::io_service& io) :m_signalSet(io)
	{
		m_signalSet.add(SIGINT);
		m_signalSet.add(SIGTERM);
	}

	~SignalHandler()
	{
		m_signalSet.clear();
	}

	void sync_wait()
	{
		m_signalSet.async_wait([this](asio::error_code ec, int signo)
			{
//			std::function<void (int)> waitHandler
//			waitHandler(signo);
			switch (signo)
			{
			case SIGINT:
				{
					LOG_DEBUG("recv SIGINT signal,exit...");
					std::exit(0);
				}			
				break;

			case SIGTERM:
				{
					LOG_DEBUG("recv SIGTERM signal,exit...");
					std::exit(SIGTERM);
				}
				break;

			default:
				break;
			}
			});
	}

private:
	asio::signal_set	m_signalSet;
};


template<class Task, class... Args>
class IcsTimer {

public:
	IcsTimer(int n, Task&& t, Args&&... args)
	{
		static const int count = sizeof...(Args);

		std::function<typename std::result_of<Task(Args...)>::type ()> t2;

		auto task = std::bind(std::forward<Task>(t), std::forward<Args...>(args...));

		task();
	}


private:
	std::function <typename std::result_of<Task(Args...)>::type ()> m_taskList;

};


#include <chrono>

template<class T>
class TimerThread {
public:
	typedef std::chrono::high_resolution_clock clock_t;

	struct TimerInfo {
	public:
		template<class Arg>
		TimerInfo(clock_t::time_point tp, Arg&& arg)
			: m_timePoint(tp)
			, m_user(std::forward<Arg>(arg))
		{

		}

		template<class Arg1,class Arg2>
		TimerInfo(clock_t::time_point tp, Arg1&& arg1, Arg2&& arg2)
			: m_timePoint(tp)
			, m_user(std::forward<Arg1>(arg1), std::forward<Arg2>(arg2))
		{

		}

		clock_t::time_point	m_timePoint;
		T	m_user;
	};

	void timerLoop()
	{
		for (;;)
		{
			std::unique_lock <std::mutex> lock(m_mutex);
			while (m_running && m_timerList.empty())
			{
				m_condition.wait(lock);
			}

			if (!m_running)
			{
				return;
			}

			if (m_sort)
			{
				std::sort(m_timerList.begin(),
					m_timerList.end(), 
					[](const TimerInfo& t1, const TimerInfo& t2)
				{
					return t1.m_timePoint > t2.m_timePoint;
				});
				m_sort = false;
			}

			auto now = clock_t::now();
			auto expire = m_timerList.back().m_timePoint;
			if (expire > now)	// can I take a nap?
			{
				m_condition.wait_for(lock, expire - now);
			}

		}
	}

private:
	std::unique_ptr<std::thread>	m_thread;
	std::vector<TimerInfo>			m_timerList;
	std::mutex				m_mutex;
	std::condition_variable	m_condition;
	bool m_sort;
	bool m_running;
};



}

#endif	// _SIGNAL_HANDLER_H