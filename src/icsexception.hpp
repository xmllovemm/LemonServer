

#ifndef _ICS_EXCEPTION_H
#define _ICS_EXCEPTION_H

#include <exception>
#include <string>

namespace ics {

class IcsException : public std::exception {
public:
	IcsException(const char* msg) : std::exception(msg)
	{

	}

	IcsException& operator << (const char* msg)
	{
		return *this;
	}

	IcsException& operator << (const std::string& msg)
	{
		return *this;
	}
};

}

#endif	// _ICS_EXCEPTION_H