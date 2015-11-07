

#ifndef _UTIL_H
#define _UTIL_H

#include <stdint.h>
#include <cstddef>
#include <string>

namespace ics {

uint32_t crc32_code(const void* buf, std::size_t size);


}
#endif	// _UTIL_H