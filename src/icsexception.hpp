

#ifndef _ICS_EXCEPTION_H
#define _ICS_EXCEPTION_H

#include <exception>
#include <string>
#include <cstdio>
#include <stdarg.h>


namespace ics {

class IcsException{
public:
	IcsException(const char* format, ...)
	{
		va_list args;
		va_start(args, format);

//		va_arg(args, int);

		va_end(args);
	}

	IcsException& operator << (const char* msg)
	{
		return *this;
	}

	IcsException& operator << (const std::string& msg)
	{
		return *this;
	}

private:
	char message[512];
};

}

#endif	// _ICS_EXCEPTION_H