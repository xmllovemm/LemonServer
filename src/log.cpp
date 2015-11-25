
#include "log.hpp"

#ifdef LOG_USE_LOG4CPLUS
#include <log4cplus/logger.h>
#include <log4cplus/configurator.h>

log4cplus::Logger icsLogger = log4cplus::Logger::getRoot();
#endif

void init_log(const char* log_config)
{
#ifdef LOG_USE_LOG4CPLUS
	log4cplus::initialize();
	log4cplus::PropertyConfigurator::doConfigure(LOG4CPLUS_TEXT(log_config));
#endif
}

void set_log_level()
{
	
}
