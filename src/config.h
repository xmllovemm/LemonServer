/*
	all config

lambda function:
[=t,&f](int a, char c) mutable throw() -> int
{

return 1;
}
[=t,&f]introducer�������:�趨��ֵ���ʱ���t,�����÷��ʱ���f
(int a, char c)parameter-declaration-list�����б�,1.�����б�����Ĭ�ϲ���	2.�����ǿɱ�����б�	3.���еĲ��������и�������
mutable�ܷ��޸Ĳ���ı���,����ڲ����б������� mutable�����ʾ���ʽ�����޸İ�ֵ������ⲿ�����Ŀ���
throw()���ܻ��ܳ����쳣
int��������ֵ����
{}����ʵ��


const vector<int> v(2);
auto n = v[0];

decltype(n) b = 1;	// b is [const int&] type
*/

#ifndef _ICS_CONFIG_H
#define _ICS_CONFIG_H


// define for asio
#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

// soft version
#define ICS_VERSION 0x10

// protocol info
#define ICS_HEAD_PROTOCOL_NAME "ICS#"	// protocol name
#define ICS_HEAD_PROTOCOL_NAME_LEN (sizeof(ICS_HEAD_PROTOCOL_NAME)-1)
#define ICS_HEAD_PROTOCOL_VERSION 0x0101	// protocol version


#include <iostream>

#define LOG_DEBUG(msg) \
do \
{\
	std::cout << msg << std::endl; \
} while (0)



#endif	// end _ICS_CONFIG_H


