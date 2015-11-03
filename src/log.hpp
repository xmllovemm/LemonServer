

#ifndef _LOG_h
#define _LOG_h

#include <iostream>

//#define LOG_FORMAT "|" << __FILE__ << ":" << __LINE__ << "|" 
#define LOG_FORMAT "|"

#define LOG_INNER(level,msg) do{\
							std::cout << level << LOG_FORMAT << msg << std::endl; \
						}while (0)

#define LOG_DEBUG(msg) LOG_INNER("[DEBUG]", msg)
#define LOG_WARN(msg)  LOG_INNER("[WARN]", msg)
#define LOG_ERROR(msg)  LOG_INNER("[ERROR]", msg)
#define LOG_FATAL(msg)  LOG_INNER("[FATAL]", msg)


#endif	// _LOG_h