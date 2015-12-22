

#ifndef _TIMER_H
#define _TIMER_H

#include <chrono>
#include <functional>
#include <thread>
#include <memory>
#include <mutex>
#include <list>
#include <array>

/*
Duration:һ��ʱ������������¼ʱ�䳤�ȣ����Ա�ʾ�����ӡ������ӻ��߼���Сʱ��ʱ����

Clocks:��ǰ��ϵͳʱ�ӣ��ڲ���time_point, duration, Rep, Period����Ϣ������Ҫ������ȡ��ǰʱ�䣬�Լ�ʵ��time_t��time_point���໥ת��

Time point:һ��ʱ��㣬������ȡ1970.1.1�����������͵�ǰ��ʱ��, ������һЩʱ��ıȽϺ��������㣬���Ժ�ctime����������ʾʱ�䡣
	time_point����Ҫclock����ʱ��time_point��һ������time_from_eproch()�������1970��1��1�յ�time_pointʱ�侭����duration
*/

class Timer {
public:
	typedef std::function<int ()> timeout;

	Timer();

	~Timer();

	/// ���һ����ʱ����������interval���ִ��handler
	void add(int interval, timeout handler);

	/// ������ʱ��
	void start();

	/// ֹͣ��ʱ�� 
	void stop();
	
private:
	struct Task{
		Task(int inter, timeout hand) : interval(inter), handler(hand){}
		int interval;
		timeout handler;
	};

private:
	void loop();

private:
	std::unique_ptr<std::thread>	m_timerThread;
	bool			m_running;

	std::list<Task>	m_taskList;
	std::recursive_mutex	m_taskListLock;
	

//	std::vector<int>	m_timeWhell;
};


/// WheelCount:ʱ����һȦ�Ŀ̶�; Tick�������̶ȵļ��ʱ��(��)
template<std::size_t WheelCount, std::size_t Tick = 1>
class TimingWheel {
public:
	using TimeOutHandler = std::function<void (void)>;

	TimingWheel()
	{

	}

	~TimingWheel()
	{
		stop();
	}

	/// ���һ��interval��tick��ʱ������th
	void add(std::size_t interval, TimeOutHandler&& th)
	{
		int positon = (interval + m_currentPoint) % WheelCount;
		int count = interval / WheelCount;
		m_wheel[positon].add(count, std::forward<TimeOutHandler>(th));
	}

	/// ����
	void start()
	{
		stop();
		m_loopFlag = true;
		m_loopThread.reset(new std::thread([this](){
			this->loop();
		}));
	}
	
	/// ֹͣ
	void stop()
	{
		m_loopFlag = false;
		if (m_loopThread)
		{
			m_loopThread->join();
		}
	}

private:
	class TaskList {
	public:
		/// ���һ��count�κ���õ�����
		void add(int count, TimeOutHandler&& th)
		{
			std::lock_guard<std::mutex> lock(m_workListLock);
			m_workList.emplace_front(count, std::forward<TimeOutHandler>(th));
		}

		/// ���ö��е�ȫ������
		void timeWork()
		{
			std::lock_guard<std::mutex> lock(m_workListLock);
			for (auto it = m_workList.begin(); it != m_workList.end();)
			{
				if (--it->count <= 0)
				{
					it->handler();
					it = m_workList.erase(it);
				}
				else
				{
					it++;
				}
			}
		}

	private:
		struct TimeWork{
			TimeWork(int c, TimeOutHandler&& th) :count(c), handler(std::forward<TimeOutHandler>(th)){}
			int				count;
			TimeOutHandler	handler;
		};

		std::mutex m_workListLock;
		std::list<TimeWork> m_workList;
	};

	/// ������ѭ��
	void loop()
	{
		while (m_loopFlag)
		{
			std::this_thread::sleep_for(std::chrono::seconds(Tick));
			m_wheel[m_currentPoint++%m_wheel.size()].timeWork();
		}
	}

private:
	std::unique_ptr<std::thread>		m_loopThread;
	std::array<TaskList, WheelCount>	m_wheel;
	bool m_loopFlag = false;
	/// ָ��ĵ�ǰ�̶�ֵ
	std::size_t m_currentPoint = 0;
};


#endif	// _TIMER_H