

#ifndef _ICS_PROTOCOL_H
#define _ICS_PROTOCOL_H

#include "config.hpp"
#include "util.hpp"
#include "otlv4.h"
#include <cstdint>
#include <string>
#include <stdexcept>
#include <iostream>
#include <exception>
#include <array>
#include <chrono>
#include <ctime>

using namespace std;


namespace ics{

namespace protocol {

#ifndef BIG_ENDIAN
#define BIG_ENDIAN		1
#endif
#ifndef LITTLE_ENDIAN
#define LITTLE_ENDIAN	2
#endif

// system byte order
#if 0
#define SYSTEM_BYTE_ORDER BIG_ENDIAN
#else
#define SYSTEM_BYTE_ORDER LITTLE_ENDIAN
#endif

// protocol byte order
#define PROTOCOL_BYTE_ORDER LITTLE_ENDIAN



inline uint16_t ics_byteorder(uint16_t n)
{
#if PROTOCOL_BYTE_ORDER == SYSTEM_BYTE_ORDER
	return n;
#else
	return ((n & 0xff) << 8) | ((n >> 8) & 0xff);
#endif
}

inline uint32_t ics_byteorder(uint32_t n)
{
#if PROTOCOL_BYTE_ORDER == SYSTEM_BYTE_ORDER
	return n;
#else
	return ((n & 0x000000ff) << 24) | ((n & 0x0000ff00) << 8) | ((n & 0x00ff0000) >> 8) | ((n & 0xff000000) >> 24);
#endif
}

inline uint64_t ics_byteorder(uint64_t n)
{
#if PROTOCOL_BYTE_ORDER == SYSTEM_BYTE_ORDER
	return n;
#else
	return ((n & 0x000000ff) << 24) | ((n & 0x0000ff00) << 8) | ((n & 0x00ff0000) >> 8) | ((n & 0xff000000) >> 24);
#endif
}


// protocol info
#define ICS_HEAD_PROTOCOL_NAME "ICS#"	// protocol name
#define ICS_HEAD_PROTOCOL_NAME_LEN (sizeof(ICS_HEAD_PROTOCOL_NAME)-1)
#define ICS_HEAD_PROTOCOL_VERSION 0x0101	// protocol version

// is ack package
#define ICS_HEAD_ATTR_ACK_FLAG 1

// need response flag
#define ICS_HEAD_ATTR_NEEDRESPONSE 1

// crc data type
typedef uint32_t	ics_crccode_t;

// string of one byte length
typedef std::string	ShortString;

// string of two bytes length
class LongString : public std::string
{
public:
	LongString() : std::string(){}
	LongString(const char* str) : std::string(str){}
	LongString(const std::string& str) : std::string(str){}
	LongString(std::string&& str) : std::string(std::forward<std::string>(str)){}

	LongString& operator = (const char* str)
	{
		*this = str;
		return *this;
	}

	LongString& operator = (const std::string& str)
	{
		*this = str;
		return *this;
	}

	LongString& operator = (std::string&& str)
	{
		*this = std::forward<std::string>(str);
		return *this;
	}


	operator std::string()
	{
		return *(std::string*)this;
	}
};


// 与终端交互消息ID枚举
enum IcsMessageId {
	terminal_auth_request = 0x0101,			// 端认证请求
	terminal_auth_response = 0x0102,			// 服务端认证应答

	// 升级功能
	terminal_upgrade_request = 0x0201,		// 服务端发起升级请求
	terminal_upgrade_deny = 0x0202,			// 终端拒绝升级请求
	terminal_upgrade_agree = 0x0203,			// 终端接收升级请求
	terminal_upgrade_file_request = 0x0204,	// 终端向中心索要升级文件片段
	terminal_upgrade_not_found = 0x0205,		// 中心通知终端当前无升级事务
	terminal_upgrade_file_response = 0x0206,	// 中心传输升级文件片段
	terminal_upgrade_result_report = 0x0207,	// 终端报告升级文件传输结果
	terminal_upgrade_cancel = 0x0208,			// web取消升级事务
	terminal_upgrade_cancel_ack = 0x0209,		// 终端确认取消升级事务

	// 状态上传
	terminal_std_status_report = 0x0301,			// 标准状态上报
	terminal_def_status_report = 0x0401,			// 自定义状态上报

	// 事件上报功能
	terminal_event_report = 0x0501,			// 事件上报

	// 参数查询
	terminal_param_query_request = 0x0601,			// 中心查询参数
	terminal_param_query_response = 0x0602,			// 终端回应参数查询

	// 终端主动上报参数修改
	terminal_param_alter_report = 0x0701,		// 终端主动上报参数修改

	// 中心修改参数
	terminal_param_modiy_reuest = 0x0801,		// 中心发起修改请求
	terminal_param_modiy_response = 0x0802,	// 终端回应参数修改

	// 业务上报功能
	terminal_bus_report = 0x0901,				// 业务上报

	// 时钟同步功能
	terminal_datetime_sync_request = 0x0a01,	// 终端发送时钟同步请求
	terminal_datetime_sync_response = 0x0a02,	// 中心应答始终同步

	// 终端发送心跳
	terminal_heartbeat = 0x0b01,				// 终端发送心跳到中心

	terminal_MAX,
};

// 与web服务器消息ID枚举
enum WebMessageId{
	web_send_terminal = 0x2001,
};

// 与推送服务器消息ID枚举
enum PushMessageId{
	pushsystem_request = 0x3001,
};

// 加密方式
enum EncryptionType
{
	EncryptionType_none = 0,
	EncryptionType_aes = 1,
	EncryptionType_rsa = 2
};


#pragma pack(1)
// 协议时间定义
typedef struct
{
	uint16_t	year;
	uint8_t		month;
	uint8_t		day;
	uint8_t		hour;
	uint8_t		miniute;
	union {
		uint16_t sec_data;
		struct {
			uint16_t second : 6;
			uint16_t milesecond : 10;
		};
	};
}IcsDataTime;


// 消息头
template<class CrcCodeType, bool BigEndian = true>
class IcsMsgHead
{
public:
	static const std::size_t CrcCodeSize = sizeof(CrcCodeType);

	void verify(const void* buf, std::size_t len) const throw(std::logic_error)
	{
		if (std::memcmp(name, ICS_HEAD_PROTOCOL_NAME, ICS_HEAD_PROTOCOL_NAME_LEN) != 0)
		{
			throw std::logic_error("protocol name error");
		}
		if (getVersion() != ICS_HEAD_PROTOCOL_VERSION)
		{
			throw std::logic_error("protocol version error");
		}
		if (getLength() != len)
		{
			throw std::logic_error("protocol length error");
		}
		if (getCrcCode() != crc32_code(buf, len - CrcCodeSize))
		{
			throw std::logic_error("protocol crc code error");
		}
	}

	// request
	void init(uint16_t id, bool response = true)
	{
		std::memset(this, 0, sizeof(IcsMsgHead));
		this->setMsgID(id);
		setFlag(0, ICS_HEAD_ATTR_ACK_FLAG, response ? 1 : 0);
	}

	// response
	void init(uint16_t id, uint16_t ack_no)
	{
		std::memset(this, 0, sizeof(IcsMsgHead));
		this->setMsgID(id);
		this->setAckNum(ack_no);
		setFlag(0, !ICS_HEAD_ATTR_ACK_FLAG, 0);
	}

	// set 0
	void clean()
	{
		memset(this, 0, sizeof(IcsMsgHead));
	}

	void setMsgID(uint16_t id)
	{
		this->id = ics_byteorder(id);
	}

	void setLength(uint16_t len)
	{
		this->length = ics_byteorder(len);
	}

	void setSendNum(uint16_t num)
	{
		this->send_num = ics_byteorder(num);
	}

	void setAckNum(uint16_t num)
	{
		this->ack_num = ics_byteorder(num);
	}

	void setFlag(int encrypt, int ack, int response)
	{
		this->encrypt = encrypt;
		this->ack = ack;
		this->response = response;
		this->flag_data = ics_byteorder(this->flag_data);
	}

	void setCrcCode()
	{
		ics_crccode_t code = 0;
		*(ics_crccode_t*)((char*)this + getLength() - sizeof(ics_crccode_t)) = ics_byteorder(code);
	}

	uint16_t getMsgID() const
	{
		return ics_byteorder(this->id);
	}

	uint16_t getLength() const
	{
		return ics_byteorder(this->length);
	}

	uint16_t getSendNum() const
	{
		return ics_byteorder(this->send_num);
	}

	uint16_t getAckNum() const
	{
		return ics_byteorder(this->ack_num);
	}

	uint16_t getFlag() const
	{
		return ics_byteorder(this->id);
	}

	uint16_t getVersion() const
	{
		return ics_byteorder(this->version);
	}

	CrcCodeType getCrcCode() const
	{
		return ics_byteorder(*(CrcCodeType*)(this->name + getLength() - CrcCodeSize));
	}
private:
	char		name[ICS_HEAD_PROTOCOL_NAME_LEN];		// 协议名称
	uint16_t	version;	// 版本号
	uint16_t	length;	// 消息长度
	union {
		uint16_t flag_data;
		struct {
			uint8_t encrypt : 4;	// 加密模式
			uint8_t reserved1 : 4;	// 保留
			uint8_t ack : 1;		// 0:请求包,1:应答包
			uint8_t response:1;		// 当前包为请求包时,0:当前包不需要应答,1:当前包需要应答
//			uint16_t reserverd2:6;
		};
	};
	uint16_t	send_num;	// 发送流水号
	uint16_t	ack_num;	// 应答流水号
	uint16_t	id;		// 消息id
};

template<class CrcCodeType = uint32_t>
class SubCommMsgHead
{
public:
	static const std::size_t CrcCodeSize = sizeof(CrcCodeType);

	void init()
	{
		std::memcpy(&m_name, "DT", 2);
		m_version = 0x0101;
	}

	void verify(void* buf, std::size_t len) throw(std::logic_error)
	{

	}

private:
	uint16_t m_name;
	uint16_t m_version;
	uint16_t m_length;
	uint16_t m_serial;
	uint16_t m_id;
};
#pragma pack()

// get current time
void getIcsNowTime(IcsDataTime& dt);

otl_stream& operator<<(otl_stream& s, const IcsDataTime& dt);

// 消息处理
template<class ProtocolHead>
class ProtocolStream
{
public:
	ProtocolStream(void* buf, std::size_t length)
	{
		reset(buf, length);
	}

	virtual ~ProtocolStream()
	{

	}

	// set current to the start
	inline void rewind()
	{
		m_pos = (uint8_t*)(m_head + 1);
	}

	// reset the buffer
	inline void reset(void* buf, std::size_t length)
	{
		if (buf == nullptr || length == 0)
		{
			throw std::logic_error("empty buff cann't init ProtocolStream");
		}
		m_head = (ProtocolHead*)buf;
		m_pos = (uint8_t*)(m_head + 1);
		m_end = (uint8_t*)buf + length;
	}

	// get the protocol head
	inline ProtocolHead* getHead() const
	{
		return m_head;
	}

	// get total size
	inline std::size_t size() const
	{
		return m_end - (uint8_t*)m_head;
	}

	// get write size
	inline std::size_t length() const
	{
		return m_pos - (uint8_t*)m_head;
	}

	// there is no more data to get
	inline bool empty() const
	{
		return m_pos == (uint8_t*)(m_head + 1);
	}

	// 
	bool appendData(void* buf, std::size_t& len)
	{
		
		std::size_t need_size = m_head->getLength();
		if (true)
		{

		}

		return true;
	}

	// -----------------------set data----------------------- 
	void serailzeToData()
	{
		m_head->setLength(m_pos - (uint8_t*)m_head + ProtocolHead::CrcCodeSize);
		*this << crc32_code(m_head, m_pos - (uint8_t*)m_head);
	}

	ProtocolStream& operator << (uint8_t data) throw(std::overflow_error)
	{
		if (sizeof(data) > leftSize())
		{
			throw overflow_error("OOM to set uint8_t data");
		}

		*m_pos++ = data;
		return *this;
	}

	ProtocolStream& operator << (uint16_t data) throw(std::overflow_error)
	{
		if (sizeof(data) > leftSize())
		{
			throw overflow_error("OOM to set uint16_t data");
		}

		*(uint16_t*)m_pos = ics_byteorder(data);
		m_pos += sizeof(data);
		return *this;
	}

	ProtocolStream& operator << (uint32_t data) throw(std::overflow_error)
	{
		if (sizeof(data) > leftSize())
		{
			throw overflow_error("OOM to set uint16_t data");
		}

		*(uint32_t*)m_pos = ics_byteorder(data);
		m_pos += sizeof(data);
		return *this;
	}

	ProtocolStream& operator << (const IcsDataTime& data) throw(std::overflow_error)
	{
		if (sizeof(data) > leftSize())
		{
			throw overflow_error("OOM to set IcsDataTime data");
		}

		*this << data.year << data.month << data.day << data.hour << data.miniute << data.sec_data;
		return *this;
	}

	ProtocolStream& operator << (const ShortString& data) throw(std::overflow_error)
	{
		if (data.length() > leftSize())
		{
			throw overflow_error("OOM to put string data");
		}
		*this << (uint8_t)data.length();
		memcpy(m_pos, data.data(), data.length());
		m_pos += data.length();
		return *this;
	}

	ProtocolStream& operator << (const LongString& data) throw(std::overflow_error)
	{
		if (data.length() + sizeof(uint16_t) > leftSize())
		{
			throw underflow_error("OOM to set long string data");
		}
		*this << (uint16_t)data.length();
		memcpy(m_pos, data.data(), data.length());
		m_pos += data.length();
		return *this;
	}

	ProtocolStream& operator << (const ProtocolStream& data) throw(std::overflow_error)
	{
		if (data.leftSize() > leftSize())
		{
			throw overflow_error("OOM to put ProtocolStream data");
		}
		std::memcpy(m_pos, data.m_pos, data.leftSize());
		m_pos += data.leftSize();
		return *this;
	}

	void verify() const throw(std::logic_error)
	{
		// verify head
		m_head->verify(m_head, size());
	}

	ProtocolStream& operator >> (uint8_t& data) throw(std::underflow_error)
	{
		if (sizeof(data) > leftSize())
		{
			throw underflow_error("OOM to get uint8_t data");
		}
		data = *m_pos++;
		return *this;
	}

	ProtocolStream& operator >> (uint16_t& data) throw(std::underflow_error)
	{
		if (sizeof(data) > leftSize())
		{
			throw underflow_error("OOM to get uint16_t data");
		}
		data = ics_byteorder(*(uint16_t*)m_pos);
		m_pos += sizeof(data);
		return *this;
	}

	ProtocolStream& operator >> (uint32_t& data) throw(std::underflow_error)
	{
		if (sizeof(data) > leftSize())
		{
			throw underflow_error("OOM to get uint32_t data");
		}
		data = ics_byteorder(*(uint32_t*)m_pos);
		m_pos += sizeof(data);
		return *this;
	}

	ProtocolStream& operator >> (float& data) throw(std::underflow_error)
	{
		if (sizeof(data) > leftSize())
		{
			throw underflow_error("OOM to get float data");
		}
#if PROTOCOL_BYTE_ORDER == SYSTEM_BYTE_ORDER
		data = *(float*)m_pos;
#else
		uint32_t tmp = ics_byteorder(*(uint32_t*)m_pos);
		data = *(float*)&tmp;
#endif
		m_pos += sizeof(data);
		return *this;
	}

	ProtocolStream& operator >> (IcsDataTime& data) throw(std::underflow_error)
	{
		if (sizeof(data) > leftSize())
		{
			throw underflow_error("OOM to get IcsDataTime data");
		}
		*this >> data.year >> data.month >> data.day >> data.hour >> data.miniute >> data.sec_data;
		return *this;
	}

	ProtocolStream& operator >> (ShortString& data) throw(std::underflow_error)
	{
		uint8_t len = 0;
		*this >> len;

		if (len > leftSize())
		{
			throw underflow_error("OOM to get string data");
		}
		data.assign((char*)m_pos, len);
		m_pos += len;
		return *this;
	}

	ProtocolStream& operator >> (LongString& data) throw(std::underflow_error)
	{
		uint16_t len;
		*this >> len;
		if (len > leftSize())
		{
			throw underflow_error("OOM to get long string data");
		}

		data.assign((char*)m_pos, len);
		m_pos += len;
		return *this;
	}

	void assertEmpty() const throw(std::logic_error)
	{
		if (leftSize() != 0)
		{
			char buff[64];
			std::sprintf(buff, "superfluous data:%d bytes", leftSize());
			throw std::logic_error(buff);
		}
	}

private:
	inline std::size_t leftSize() const
	{
		return m_end - m_pos - ProtocolHead::CrcCodeSize;
	}

protected:
	// point to the protocol head
	ProtocolHead*	m_head;

	// point to the current operating address of buffer
	uint8_t*		m_pos;

	// point to the end address of buffer
	uint8_t*		m_end;

};



}	// end protocol
}	// end ics

#endif	// end _ICS_PROTOCOL_H