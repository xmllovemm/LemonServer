

#ifndef _TIMER_H
#define _TIMER_H

#include <chrono>
#include <functional>
#include <thread>
#include <memory>
#include <mutex>
#include <list>

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
	typedef struct {
		int interval;
		timeout handler;
	}Task;

private:
	void loop();

private:
	std::unique_ptr<std::thread>	m_timerThread;
	bool			m_running;

	std::list<Task>	m_taskList;
	std::recursive_mutex	m_taskListLock;
	

//	std::vector<int>	m_timeWhell;
};

#endif	// _TIMER_H