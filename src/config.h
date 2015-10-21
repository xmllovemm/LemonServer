/*
	all config

lambda function:
[=t,&f](int a, char c) mutable throw() -> int
{

return 1;
}
[=t,&f]introducer捕获规则:设定按值访问变量t,按引用访问变量f
(int a, char c)parameter-declaration-list变量列表,1.参数列表不能有默认参数	2.不能是可变参数列表	3.所有的参数必须有个变量名
mutable能否修改捕获的变量,如果在参数列表后加上了 mutable，则表示表达式可以修改按值捕获的外部变量的拷贝
throw()可能会跑出的异常
int函数返回值类型
{}函数实体


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
