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

// soft version
#define ICS_VERSION 0x10


// c++ asio
#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#include <asio.hpp>
#endif

/*
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
*/

// byte order
#define ICS_USE_LITTLE_ENDIAN

#endif	// end _ICS_CONFIG_H
