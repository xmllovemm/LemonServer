

#ifndef _ICS_PROTOCOL_H
#define _ICS_PROTOCOL_H

#include "config.hpp"
#include "otlv4.h"
#include "icsexception.hpp"
#include "mempool.hpp"
#include <cstring>
#include <cstdint>
#include <string>
#include <stdexcept>
#include <iostream>
#include <exception>
#include <chrono>
#include <ctime>
#include <cstddef>

using namespace std;


namespace ics{

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


uint8_t ics_byteorder(uint8_t n);

uint16_t ics_byteorder(uint16_t n);

uint32_t ics_byteorder(uint32_t n);

uint64_t ics_byteorder(uint64_t n);

float ics_byteorder(float n);

double ics_byteorder(double n);

std::time_t ics_byteorder(std::time_t n);


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


// ICS消息ID枚举
// T--terminal, C--center, W--web, P--pushsystem
enum MessageId {
	MessageId_min_0x0000 = 0x0,

	// ------ T and C ------ //
	T2C_min_0x0100 = 0x0100,
	// 认证请求
	T2C_auth_request_0x0101 = 0x0101,			
	// 认证应答
	C2T_auth_response_0x0102 = 0x0102,			

	// 发起升级请求
	C2T_upgrade_request_0x0201 = 0x0201,
	// 拒绝升级
	T2C_upgrade_deny_0x0202 = 0x0202,			
	// 同意升级
	T2C_upgrade_agree_0x0203 = 0x0203,		
	// 索要升级文件片段
	T2C_upgrade_file_request_0x0204 = 0x0204,	
	// 当前无升级事务
	C2T_upgrade_not_found_0x0205 = 0x0205,		
	// 升级文件片段
	C2T_upgrade_file_response_0x0206 = 0x0206,
	// 升级文件传输结果
	T2C_upgrade_result_report_0x0207 = 0x0207,
	// 取消升级事务
	C2T_upgrade_cancel_0x0208 = 0x0208,			
	// 确认取消升级事务
	T2C_upgrade_cancel_ack_0x0209 = 0x0209,		
	
	// 标准状态上报
	T2C_std_status_report_0x0301 = 0x0301,
	// 自定义状态上报
	T2C_def_status_report_0x0401 = 0x0401,			
	
	// 事件上报
	T2C_event_report_0x0501 = 0x0501,			

	// 中心查询参数
	C2T_param_query_request_0x0601 = 0x0601,			
	// 终端回应参数查询
	T2C_param_query_response_0x0602 = 0x0602,			

	// 终端主动上报参数修改
	T2C_param_alter_report_0x0701 = 0x0701,		

	// 中心发起修改请求
	C2T_param_modiy_reuest_0x0801 = 0x0801,		
	// 终端回应参数修改
	T2C_param_modiy_response_0x0802 = 0x0802,	

	// 业务上报
	T2C_bus_report_0x0901 = 0x0901,				
	// gps上报
	T2C_gps_report_0x0902 = 0x0902,				

	// 终端发送时钟同步请求
	T2C_datetime_sync_request_0x0a01 = 0x0a01,	
	// 中心应答始终同步
	C2T_datetime_sync_response_0x0a02 = 0x0a02,	

	// 终端发送心跳
	T2C_heartbeat_0x0b01 = 0x0b01,				

	// 终端上报日志
	T2C_log_report_0x0c01 = 0x0c01,

	T2C_max,


	// ------ W and C ------ //
	W2C_min_0x2000 = 0x2000,
	// 发给ICS终端
	W2C_send_to_ics_terminal_0x2001 = 0x2001,
	// 连接远端服务器请求
	W2C_connect_remote_request_0x2002 = 0x2002,
	// 断开某个远端服务器连接
	W2C_disconnect_remote_0x2003 = 0x2003,
	// 发给远程代理服务器对应终端
	W2C_send_to_remote_terminal_0x2004 = 0x2004,

	W2C_max,

	// ------ P and C ------ //
	// 推送消息
	C2P_push_message_0x3001 = 0x3001,

	// ------ C and C ------ //
	// 中心通信服务器认证请求1
	C2C_auth_request1_0x4001 = 0x4001,
	// 认证结果
	C2C_auth_response_0x4002 = 0x4002,
	// 中心通信服务器认证请求2
	C2C_auth_request2_0x4003 = 0x4003,
	// 中心通信服务器通知远程服务器转发终端消息
	C2C_forward_to_terminal_0x4004 = 0x4004,
	// 远程服务器回应转发结果
	C2C_forward_response_0x4005 = 0x4005,
	// 子服务器上报终端上下线消息
	C2C_terminal_onoff_line_0x4006 = 0x4006,
	// 子服务器上报终端消息
	C2C_forward_to_ics_0x4007 = 0x4007,
	// 两者心跳消息
	C2C_heartbeat_0x4008 = 0x4008,

	MessageId_max,
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

// ICS消息头
class IcsMsgHead
{
public:
	static const std::size_t CrcCodeSize = sizeof(uint32_t);

	IcsMsgHead()
	{
		std::memset(this, 0, sizeof(IcsMsgHead));
		std::memcpy(this->name, ICS_HEAD_PROTOCOL_NAME, sizeof(this->name));
		this->version = ics_byteorder((uint16_t)ICS_HEAD_PROTOCOL_VERSION);
	}

	void verify(const void* buf, std::size_t len) const throw(IcsException);

	// set 0
	void clean();

	void setMsgID(uint16_t id);

	void setLength(uint16_t len);

	void setSendNum(uint16_t num);

	void setAckNum(uint16_t num);

	void setFlag(int encrypt, int ack, int needResponse);

	void setCrcCode();

	MessageId getMsgID() const;

	uint16_t getLength() const;

	bool isResponse() const;

	bool needResposne() const;

	uint16_t getSendNum() const;

	uint16_t getAckNum() const;

	uint16_t getFlag() const;

	uint16_t getVersion() const;

	uint32_t getCrcCode() const;
private:
	uint8_t		name[ICS_HEAD_PROTOCOL_NAME_LEN];		// 协议名称
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
#pragma pack()

// get current time
void getIcsNowTime(IcsDataTime& dt);


otl_stream& operator<<(otl_stream& s, const IcsDataTime& dt);

otl_stream& operator<<(otl_stream& s, const LongString& dt);


/// ICS消息处理类
class ProtocolStream
{
public:
	enum OptType {
		readType,
		writeType
	};

	ProtocolStream(OptType type, void* buf, std::size_t length);

	ProtocolStream(OptType type, const MemoryChunk& chunk);

	ProtocolStream(const ProtocolStream& rhs, const MemoryChunk& chunk);

	~ProtocolStream();

	/// 调用该接口以后不可读写操作
	MemoryChunk toMemoryChunk();

	/// 重置操作位置
	void rewind()
	{
		m_pos = m_start + sizeof(IcsMsgHead);
	}

	/// 跳过该类型的数据
	template<class T>
	ProtocolStream& moveForward() throw(IcsException)
	{
		if (sizeof(T) > leftLength())
		{
			throw IcsException("can't move forward %d bytes", sizeof(T));
		}
		m_pos += sizeof(T);
		return *this;
	}

#ifdef WIN32
	/// 跳过ShortString类型的数据
	template<>
	ProtocolStream& moveForward<ShortString>() throw(IcsException)
	{
		uint8_t len;
		*this >> len;
		if (len > leftLength())
		{
			throw IcsException("can't move forward %d bytes", len);
		}
		m_pos += len;
		return *this;
	}

	/// 跳过LongString类型的数据
	template<>
	ProtocolStream& moveForward<LongString>() throw(IcsException)
	{
		uint16_t len;
		*this >> len;
		if (len > leftLength())
		{
			throw IcsException("can't move forward %d bytes", len);
		}
		m_pos += len;
		return *this;
	}
#endif

	/// 往回移动
	void moveBack(std::size_t offset) throw(IcsException);

	/// get the protocol head
	IcsMsgHead* getHead() const
	{
		return (IcsMsgHead*)m_start;
	}

	/// 移动长度
	std::size_t length() const
	{
		return m_pos - m_start;
	}

	/// 剩余长度
	std::size_t leftLength() const
	{
		return m_end - m_pos;
	}

	/// 总长度
	std::size_t size() const
	{
		return m_end - m_start;
	}

	// -----------------------write data----------------------- 
	/// 按请求消息初始化消息头
	void initHead(MessageId id, bool needResponse);

	/// 按响应消息初始化消息头
	void initHead(MessageId id, uint16_t ackNum);

	// 设置消息体长度和校验码
	void serialize(uint16_t sendNum);

	template<class T>
	ProtocolStream& operator << (const T& data) throw(IcsException)
	{
		if (sizeof(data) > leftLength())
		{
			throw IcsException("OOM to set %d bytes data", sizeof(data));
		}

		*(T*)m_pos = ics_byteorder(data);
		m_pos += sizeof(data);
		return *this;
	}

	ProtocolStream& operator << (const IcsDataTime& data) throw(IcsException);

	template<class LengthType = uint8_t>
	ProtocolStream& operator << (const char* data) throw(IcsException)
	{
		std::size_t len  = std::strlen(data);

		if (len + sizeof(LengthType) > leftLength())
		{
			throw IcsException("OOM to set %d bytes string data",len);
		}
		*this << (LengthType)len;
		memcpy(m_pos, data, len);
		m_pos += len;

		return *this;
	}

	ProtocolStream& operator << (const ShortString& data) throw(IcsException);

	ProtocolStream& operator << (const LongString& data) throw(IcsException);

	ProtocolStream& operator << (const ProtocolStream& data) throw(IcsException);

	void append(const void* data, std::size_t len);


	// -----------------------read data----------------------- 
	template<class T>
	ProtocolStream& operator >> (T& data) throw(IcsException)		
	{
		if (sizeof(data) > leftLength())
		{
			throw IcsException("OOM to get uint16_t data");
		}
		data = ics_byteorder(*(T*)m_pos);
		m_pos += sizeof(data);
		return *this;
	}

	ProtocolStream& operator >> (IcsDataTime& data) throw(IcsException);

	ProtocolStream& operator >> (ShortString& data) throw(IcsException);

	ProtocolStream& operator >> (LongString& data) throw(IcsException);

	/// 断言消息已读完
	void assertEmpty() const throw(IcsException);

private:	
	/// 操作类型
	OptType		m_optType;
	/// 起始地址
	uint8_t*	m_start;
	/// 移动指针
	uint8_t*	m_pos;
	/// 终止地址
	uint8_t*	m_end;
};

/*
/// 跳过该类型的数据
template<class T>
ProtocolStream& ProtocolStream::moveForward() throw(IcsException)
{
	if (sizeof(T) > leftLength())
	{
		throw IcsException("can't move forward %d bytes", sizeof(T));
	}
	m_pos += sizeof(T);
	return *this;
}
//*/


}	// end ics

#endif	// end _ICS_PROTOCOL_H