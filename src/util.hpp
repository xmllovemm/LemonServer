

#ifndef _UTIL_H
#define _UTIL_H

#include "icsexception.hpp"
#include <stdint.h>
#include <cstddef>
#include <string>

#ifndef WIN32
#endif

namespace ics {

uint32_t crc32_code(const void* buf, std::size_t size);

void character_convert(const char* from_code, const std::string& src, std::size_t len, const char* to_code, std::string& dest) throw (IcsException);

void be_daemon(const char* root_dir) throw (IcsException);

}
#endif	// _UTIL_H