

#ifndef _UTIL_H
#define _UTIL_H

#include <stdint.h>
#include <cstddef>
#include <string>

namespace ics {

uint32_t crc32_code(const void* buf, std::size_t size);

bool character_convert(const char* from_code, const std::string& src, std::size_t len, const char* to_code, std::string& dest);

}
#endif	// _UTIL_H