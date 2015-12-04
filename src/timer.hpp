

#ifndef _TIMER_H
#define _TIMER_H

#include <chrono>
#include <functional>
#include <thread>
#include <memory>


/*
Duration:һ��ʱ������������¼ʱ�䳤�ȣ����Ա�ʾ�����ӡ������ӻ��߼���Сʱ��ʱ����

Clocks:��ǰ��ϵͳʱ�ӣ��ڲ���time_point, duration, Rep, Period����Ϣ������Ҫ������ȡ��ǰʱ�䣬�Լ�ʵ��time_t��time_point���໥ת��

Time point:һ��ʱ��㣬������ȡ1970.1.1�����������͵�ǰ��ʱ��, ������һЩʱ��ıȽϺ��������㣬���Ժ�ctime����������ʾʱ�䡣
	time_point����Ҫclock����ʱ��time_point��һ������time_from_eproch()�������1970��1��1�յ�time_pointʱ�侭����duration
*/

class Timer {
public:
	typedef std::function<void()> timeout;

	Timer();

	~Timer();

	/// ���һ����ʱ����������interval���ִ��handler
	void add(int interval, timeout handler);

	/// ������ʱ��
	void start();

	/// ֹͣ��ʱ��
	void stop();

private:
	void loop();

private:
	std::unique_ptr<std::thread>	m_timerThread;
	bool			m_running;
};

#endif	// _TIMER_H