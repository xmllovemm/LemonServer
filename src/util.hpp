

#ifndef _UTIL_H
#define _UTIL_H

#include "icsexception.hpp"
#include <stdint.h>
#include <cstddef>
#include <string>

#ifndef WIN32
#endif

namespace ics {

/// 计算该数据段CRC32校验值
uint32_t crc32_code(const void* buf, std::size_t size);

/// 字符集转换
void character_convert(const char* from_code, const std::string& src, std::size_t len, const char* to_code, std::string& dest) throw (IcsException);

/// 成为精灵进程
void be_daemon(const char* root_dir) throw (IcsException);

/// 加密该数据段
void encrypt(void* data, std::size_t);

/// 解密该数据段
void decrypt(void* data, std::size_t);

}
#endif	// _UTIL_H