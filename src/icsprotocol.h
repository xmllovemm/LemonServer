

#ifndef _ICS_PROTOCOL_H
#define _ICS_PROTOCOL_H

#include "config.h"
#include <cstdint>
#include <string>
#include <stdexcept>
#include <iostream>

using namespace std;


#if !defined(ICS_USE_LITTLE_ENDIAN) && !defined(ICS_USE_BIG_ENDIAN)
#error must define either 'ICS_USE_LITTLE_ENDIAN' or 'ICS_USE_BIG_ENDIAN'
#endif



namespace ics{

// protocol info
#define ICS_HEAD_PROTOCOL_NAME "ICS#"	// protocol name
#define ICS_HEAD_PROTOCOL_NAME_LEN (sizeof(ICS_HEAD_PROTOCOL_NAME)-1)
#define ICS_HEAD_PROTOCOL_VERSION 0x0101	// protocol version

// is ack package
#define ICS_HEAD_ATTR_ACK_FLAG 1

// need response flag
#define ICS_HEAD_ATTR_NEEDRESPONSE 1


class IcsProtocol
{
public:
	typedef uint8_t		ics_u8_t;
	typedef uint16_t	ics_u16_t;
	typedef uint32_t	ics_u32_t;
	typedef std::string	ics_string_t;


	/*
	// 协议字节序转化
	static inline uint16_t ics_ntoh(uint16_t n)
	{
#ifdef ICS_USE_LITTLE_ENDIAN
		return n;
#else
		return ntohs(n);
#endif
	}

	static inline uint32_t ics_ntoh(uint32_t n)
	{
#ifdef ICS_USE_LITTLE_ENDIAN
		return n;
#else
		return ntohl(n);
#endif
	}

	static inline uint64_t ics_ntoh(uint64_t n)
	{
#ifdef ICS_USE_LITTLE_ENDIAN
		return n;
#else
		return ntohll(n);
#endif
	}

	static inline uint16_t ics_hton(uint16_t n)
	{
#ifdef ICS_USE_LITTLE_ENDIAN
		return n;
#else
		return ntohs(n);
#endif
	}

	static inline uint32_t ics_hton(uint32_t n)
	{
#ifdef ICS_USE_LITTLE_ENDIAN
		return n;
#else
		return ntohs(n);
#endif
	}

	static inline uint64_t ics_hton(uint64_t n)
	{
#ifdef ICS_USE_LITTLE_ENDIAN
		return n;
#else
		return ntohll(n);
#endif
	}
	*/
	template<typename T>
	static inline T ics_ntoh(T data);

	template<typename T>
	static inline T ics_hton(T data);

	// 与终端交互消息ID枚举
	enum IcsMessageId {
		IcsMsg_auth_request = 0x0101,			// 端认证请求
		IcsMsg_auth_response = 0x0102,			// 服务端认证应答

		// 升级功能
		IcsMsg_upgrade_request = 0x0201,		// 服务端发起升级请求
		IcsMsg_upgrade_deny = 0x0202,			// 终端拒绝升级请求
		IcsMsg_upgrade_agree = 0x0203,			// 终端接收升级请求
		IcsMsg_upgrade_file_request = 0x0204,	// 终端向中心索要升级文件片段
		IcsMsg_upgrade_not_found = 0x0205,		// 中心通知终端当前无升级事务
		IcsMsg_upgrade_file_response = 0x0206,	// 中心传输升级文件片段
		IcsMsg_upgrade_result_report = 0x0207,	// 终端报告升级文件传输结果
		IcsMsg_upgrade_cancel = 0x0208,			// web取消升级事务
		IcsMsg_upgrade_cancel_ack = 0x0209,		// 终端确认取消升级事务

		// 状态上传
		IcsMsg_std_status_report = 0x0301,			// 标准状态上报
		IcsMsg_def_status_report = 0x0401,			// 自定义状态上报

		// 事件上报功能
		IcsMsg_event_report = 0x0501,			// 事件上报

		// 参数查询
		IcsMsg_param_query_request = 0x0601,			// 中心查询参数
		IcsMsg_param_query_response = 0x0602,			// 终端回应参数查询

		// 终端主动上报参数修改
		IcsMsg_param_alter_report = 0x0701,		// 终端主动上报参数修改

		// 中心修改参数
		IcsMsg_param_modiy_reuest = 0x0801,		// 中心发起修改请求
		IcsMsg_param_modiy_response = 0x0802,	// 终端回应参数修改

		// 业务上报功能
		IcsMsg_bus_report = 0x0901,				// 业务上报

		// 时钟同步功能
		IcsMsg_datetime_sync_request = 0x0a01,	// 终端发送时钟同步请求
		IcsMsg_datetime_sync_response = 0x0a02,	// 中心应答始终同步

		// 终端发送心跳
		IcsMsg_heartbeat = 0x0b01,				// 终端发送心跳到中心

		IscMsg_MAX,
	};

	// 与web服务器消息ID枚举
	enum WebMessageId{
		WebMessage_send_to_terminal = 0x2001,
	};

	// 与推送服务器消息ID枚举
	enum PushMessageId{
		PushMessage_Request = 0x3001,
	};

	// 加密方式
	enum EncryptionType
	{
		EncryptionType_none = 0,
		EncryptionType_aes = 1,
		EncryptionType_rsa = 2
	};

#pragma pack(1)
	// 消息头定义
	typedef struct _IcsMsgHead
	{	
		char		name[ICS_HEAD_PROTOCOL_NAME_LEN];		// 协议名称
		uint16_t	version;	// 版本号
		uint16_t	length;	// 消息长度
		union {
			struct {
				uint8_t encrypt:4;	// 加密模式
				uint8_t reserved1:4;	// 保留
				uint8_t ack:1;		// 0:请求包,1:应答包
				uint8_t response:1;	// 当前包为请求包时,0:当前包不需要应答,1:当前包需要应答
				uint16_t reserverd2:6;
			};
		};
		uint16_t	send_num;	// 发送流水号
		uint16_t	ack_num;	// 应答流水号
		uint16_t	id;		// 消息id
	}IcsMsgHead;

	// 协议时间定义
	typedef struct _IcsDataTime
	{
		uint16_t	year;
		uint8_t		month;
		uint8_t		day;
		uint8_t		hour;
		uint8_t		miniute;
		struct {
			uint16_t second:6;
			uint16_t milesecond:10;
		};
	}IcsDataTime;
#pragma pack()

	IcsProtocol(void* buf, size_t length);

	void reset();

	inline void* addr()
	{
		return m_start;
	}

	inline size_t length()
	{
		return m_pos - m_start;
	}

	inline size_t size()
	{
		return m_end - m_start;
	}

	inline size_t leftLength()
	{
		return m_end - m_pos;
	}

	// set data
	void serailzeToData();

	void initHead(uint16_t id, uint16_t send_num, uint16_t ack_num, bool request, bool response, EncryptionType et);

	void initHead(uint16_t id, uint16_t send_num);

	void initHead(uint16_t ack_num);

	IcsProtocol& operator << (const uint8_t& data);

	IcsProtocol& operator << (const uint16_t& data);

	IcsProtocol& operator << (const IcsDataTime& data);

	IcsProtocol& operator << (const string& data);

	template<typename T = uint8_t>
	void setString(const string& data);


	// get data
	void parseFormData(void* buf, int length);

	IcsMsgHead* getHead();

	IcsProtocol& operator >> (uint8_t& data);

	IcsProtocol& operator >> (uint16_t& data);

	IcsProtocol& operator >> (IcsDataTime& data);

	IcsProtocol& operator >> (string& data);

	template<typename T = uint8_t>
	string getString();

	// calcutlate the crc code
	uint16_t getCrc32Code(uint8_t* buf, int length);

private:
	uint8_t*	m_start;
	uint8_t*	m_pos;
	uint8_t*	m_end;
};

}	// end 
#endif	// end _ICS_PROTOCOL_H