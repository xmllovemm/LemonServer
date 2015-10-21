

#ifndef _LOG_h
#define _LOG_h

#include <iostream>

#define LOG_DEBUG(msg) do{\
							std::cout << "[DEBUG]" << __FILE__ << ":" << __LINE__ << "|" << msg << std::endl;\
						}while (0)



#endif	// _LOG_h