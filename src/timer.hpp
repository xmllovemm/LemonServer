

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
Duration:一段时间间隔，用来记录时间长度，可以表示几秒钟、几分钟或者几个小时的时间间隔

Clocks:当前的系统时钟，内部有time_point, duration, Rep, Period等信息，它主要用来获取当前时间，以及实现time_t和time_point的相互转换

Time point:一个时间点，用来获取1970.1.1以来的秒数和当前的时间, 可以做一些时间的比较和算术运算，可以和ctime库结合起来显示时间。
	time_point必须要clock来计时，time_point有一个函数time_from_eproch()用来获得1970年1月1日到time_point时间经过的duration
*/

class Timer {
public:
	typedef std::function<int ()> timeout;

	Timer();

	~Timer();

	/// 添加一个超时处理任务，在interval秒后执行handler
	void add(int interval, timeout handler);

	/// 开启定时器
	void start();

	/// 停止定时器 
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


/// WheelCount:时间轮一圈的刻度; Tick：两个刻度的间隔时间(秒)
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

	/// 添加一个interval个tick后超时的任务th
	void add(std::size_t interval, TimeOutHandler&& th)
	{
		int positon = (interval + m_currentPoint) % WheelCount;
		int count = interval / WheelCount;
		m_wheel[positon].add(count, std::forward<TimeOutHandler>(th));
	}

	/// 启动
	void start()
	{
		stop();
		m_loopFlag = true;
		m_loopThread.reset(new std::thread([this](){
			this->loop();
		}));
	}
	
	/// 停止
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
		/// 添加一个count次后调用的任务
		void add(int count, TimeOutHandler&& th)
		{
			std::lock_guard<std::mutex> lock(m_workListLock);
			m_workList.emplace_front(count, std::forward<TimeOutHandler>(th));
		}

		/// 检查该队列的全部任务
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

	/// 主任务循环
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
	/// 指向的当前刻度值
	std::size_t m_currentPoint = 0;
};


#endif	// _TIMER_H