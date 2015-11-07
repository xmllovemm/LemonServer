

#include "icsprotocol.hpp"


namespace ics{

namespace protocol{

otl_stream& operator<<(otl_stream& s, const IcsDataTime& dt)
{
	otl_datetime od;
	od.year = dt.year;
	od.month = dt.month;
	od.day = dt.month;
	od.hour = dt.hour;
	od.minute = dt.miniute;
	od.second = dt.second;

	return s << od;
}


void getIcsNowTime(IcsDataTime& dt)
{
	std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	std::tm* tm = std::localtime(&t);
	dt.year = tm->tm_year + 1900;
	dt.month = tm->tm_mon + 1;
	dt.day = tm->tm_mday;
	dt.hour = tm->tm_hour;
	dt.miniute = tm->tm_min;
	dt.second = tm->tm_sec;
	dt.milesecond = 0;
}
}
}
