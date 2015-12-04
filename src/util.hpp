

#ifndef _UTIL_H
#define _UTIL_H

#include "icsexception.hpp"
#include <stdint.h>
#include <cstddef>
#include <string>

#ifndef WIN32
#endif

namespace ics {

/// ��������ݶ�CRC32У��ֵ
uint32_t crc32_code(const void* buf, std::size_t size);

/// �ַ���ת��
void character_convert(const char* from_code, const std::string& src, std::size_t len, const char* to_code, std::string& dest) throw (IcsException);

/// ��Ϊ�������
void be_daemon(const char* root_dir) throw (IcsException);

/// ���ܸ����ݶ�
void encrypt(void* data, std::size_t);

/// ���ܸ����ݶ�
void decrypt(void* data, std::size_t);

}
#endif	// _UTIL_H