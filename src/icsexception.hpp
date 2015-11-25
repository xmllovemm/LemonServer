

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
		char message[126];
		va_list args;

		va_start(args, format);
		std::vsnprintf(message, sizeof(message), format, args);
		va_end(args);
		m_message = message;
	}

	IcsException& operator << (const char* msg)
	{
		m_message += msg;
		return *this;
	}

	IcsException& operator << (const std::string& msg)
	{
		m_message += msg;
		return *this;
	}

	IcsException& format(const char* format, ...)
	{
		char message[126];
		va_list args;
		va_start(args, format);
		std::vsnprintf(message, sizeof(message), format, args);
		va_end(args);

		m_message = message;
		return *this;
	}

	const std::string& message() const
	{
		return m_message;
	}
private:
	std::string m_message;
};

}

#endif	// _ICS_EXCEPTION_H