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

in c++ 14:
auto lambda = [](auto x,auto y){return x+y;};

std::unique_ptr prt(new int(10));
auto lam = [value = ::std::move(ptr)]{return *value};

const vector<int> v(2);
auto n = v[0];

decltype(n) b = 1;	// b is [const int&] type


// OTL
timestamp ----> SQL-server:DATETIME,Oracle:DATE,Oracle 9i:TIMESTAMP   YYYY-MM-DD hh:mm:ss
:fn<unsigned/int/float/double/timestamp/char[12],in|out|inout>


*/

#ifndef _CONFIG_H
#define _CONFIG_H

// soft version
#define ICS_VERSION 0x0101


// c++ asio
#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif
#ifndef ASIO_HEADER_ONLY
#define ASIO_HEADER_ONLY
#endif

#ifdef WIN32
#include <asio.hpp>
#endif

// byte order
#define ICS_USE_LITTLE_ENDIAN

// database
#define OTL_STL
#define OTL_ODBC
#define OTL_CONNECT_POOL_ON
#define OTL_CPP_11_ON


#if defined(WIN32) || defined (WIN64)	// in windows
#pragma warning(disable:4290)
#pragma warning(disable:4996)

#else	// in linux

//#define LOG_USE_LOG4CPLUS	// log configure
#define OTL_ODBC_UNIX
#endif



// Communicate Server/Sub Server
//#define ICS_CENTER_MODE

#endif	// end _CONFIG_H
