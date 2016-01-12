

#ifndef _LOG_h
#define _LOG_h

#include "config.hpp"
#include <iostream>




#ifdef LOG_USE_LOG4CPLUS

#include <log4cplus/logger.h>
#include <log4cplus/loggingmacros.h>

extern log4cplus::Logger icsLogger;

#define LOG_DEBUG(msg)	LOG4CPLUS_DEBUG(icsLogger, msg)
#define LOG_WARN(msg)	LOG4CPLUS_WARN(icsLogger, msg)
#define LOG_INFO(msg)	LOG4CPLUS_INFO(icsLogger, msg)
#define LOG_ERROR(msg)  LOG4CPLUS_ERROR(icsLogger, msg)
#define LOG_FATAL(msg)  LOG4CPLUS_FATAL(icsLogger, msg)

#else
//#define LOG_FORMAT "|" << __FILE__ << ":" << __LINE__ << "|" 
#define LOG_FORMAT "|"

#define LOG_INNER(level,msg) do{\
							std::cout << level << LOG_FORMAT << msg << std::endl; \
						}while (0)

#define LOG_DEBUG(msg)	LOG_INNER("[DEBUG]", msg)
#define LOG_INFO(msg)	LOG_INNER("[INFO]", msg)
#define LOG_WARN(msg)	LOG_INNER("[WARN]", msg)
#define LOG_ERROR(msg)  LOG_INNER("[ERROR]", msg)
#define LOG_FATAL(msg)  LOG_INNER("[FATAL]", msg)
#endif

namespace ics {
void init_log(const char* log_config);

void set_log_level();
}

#endif	// _LOG_h