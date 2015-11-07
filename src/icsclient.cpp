
#include "config.hpp"
#include "icsclient.hpp"
#include "log.hpp"
#include "database.hpp"

#include <cstdio>

extern ics::DataBase db;
extern ics::MemoryPool mp;

namespace ics{

//----------------------------------------------------------------------------------//

AuthrizeInfo::AuthrizeInfo(const AuthrizeInfo& rhs)
{
	m_gw_id = rhs.m_gw_id;
	m_gw_pwd = rhs.m_gw_pwd;
	m_ext_info = rhs.m_ext_info;
}

AuthrizeInfo::AuthrizeInfo(AuthrizeInfo&& rhs)
	: m_gw_id(rhs.m_gw_id), m_gw_pwd(rhs.m_gw_pwd), m_ext_info(rhs.m_ext_info)
{

}

AuthrizeInfo::AuthrizeInfo(const string& gw_id, const string& gw_pwd, uint16_t device_kind, const string& ext_info)
	: m_gw_id(gw_id), m_gw_pwd(gw_pwd), m_device_kind(device_kind), m_ext_info(ext_info)
{

}


AuthrizeInfo::AuthrizeInfo(string&& gw_id, string&& gw_pwd, uint16_t device_kind, string&& ext_info)
	: m_gw_id(gw_id), m_gw_pwd(gw_pwd), m_device_kind(device_kind), m_ext_info(ext_info)
{

}


AuthrizeInfo& AuthrizeInfo::operator=(const AuthrizeInfo& rhs)
{
	m_gw_id = rhs.m_gw_id;
	m_gw_pwd = rhs.m_gw_pwd;
	m_ext_info = rhs.m_ext_info;
	return *this;
}

AuthrizeInfo& AuthrizeInfo::operator=(AuthrizeInfo&& rhs)
{
	m_gw_id = rhs.m_gw_id;
	m_gw_pwd = rhs.m_gw_pwd;
	m_ext_info = rhs.m_ext_info;
	return *this;
}


ostream& operator << (ostream& os, const AuthrizeInfo& rhs)
{
	os << "GW id: " << rhs.m_gw_id << ", pwd: " << rhs.m_gw_pwd << ", ext info: " << rhs.m_ext_info;
	return os;
}

//----------------------------------------------------------------------------------//





//*/
    
IcsClient::IcsClient(asio::ip::tcp::socket&& s, ClientManager& cm)
: TcpConnection(std::forward<asio::ip::tcp::socket>(s)), m_client_manager(cm), m_isSending(false)
{
    
}

IcsClient::~IcsClient()
{
    LOG_DEBUG(m_conn_name << " has been deleted");
}
    
void IcsClient::do_read()
{
	// wait response
	m_socket.async_read_some(asio::buffer(m_recv_buf, sizeof(m_recv_buf)),
		[this](const std::error_code& ec, std::size_t length)
		{
			// no error and handle message
			if (!ec && handleData(m_recv_buf, length))
			{
				// continue to read		
				do_read();
			}
			else
			{
				LOG_DEBUG(m_conn_name << " recv/handle data error");
				do_error();
			}
		});
}

void IcsClient::do_write()
{
	trySend();
	
}

void IcsClient::toHexInfo(uint8_t* data, std::size_t length)
{
#ifndef NDEBUG
	char buff[1024];
	for (size_t i = 0; i < length && i < sizeof(buff)/3; i++)
	{
		std::sprintf(buff + i * 3, " %02x", data[i]);
	}
	LOG_DEBUG(m_conn_name << " recv " << length << " bytes:" << buff);
#endif
}

void IcsClient::trySend()
{
	std::lock_guard<std::mutex> lock(m_sendLock);
	if (!m_isSending && !m_sendList.empty())
	{
		MemoryChunk& chunk = m_sendList.front();
		m_socket.async_send(asio::buffer(chunk.getBuff(), chunk.getUsedSize()),
			[this](const std::error_code& ec, std::size_t length)
			{
				if (!ec && length == m_sendList.front().getUsedSize())
				{
					mp.put(m_sendList.front());
					m_sendList.pop_front();
					m_isSending = false;
					trySend();
				}
				else
				{
					LOG_DEBUG(m_conn_name << " send data error");
					do_error();
				}
			});
	}
}

void IcsClient::trySend(MemoryChunk& mc)
{
//	std::lock_guard<std::mutex> lock(m_sendLock);
	m_sendList.push_back(mc);
	trySend();
}

bool IcsClient::handleData(uint8_t* buf, std::size_t length)
{
	// show debug info
	this->toHexInfo(buf, length);

	/*
	if (m_conn_name.empty())
	{
		if (length < sizeof(ProtocolHead)+ProtocolHead::CrcCodeSize)
		{
			LOG_ERROR("first package's size is not more than IcsMsgHead");
			return false;
		}

		if (head->getMsgID() != protocol::terminal_auth_request)
		{
			LOG_ERROR("first package isn't authrize message");
			return false;
		}
	}
	*/
	ProtocolStream request(buf, length);

	MemoryChunk mc = mp.get();

	ProtocolStream response(mc.getBuff(), mc.getTotalSize());

	ProtocolHead* head = request.getHead();

	try {
		handleMessage(request, response);

		if (!response.empty())
		{
			response.getHead()->setSendNum(m_send_num++);
			response.serailzeToData();
			mc.setUsedSize(response.length());
			trySend(mc);
		}
	}
	catch (std::exception& ex)
	{
		LOG_ERROR("handle message [" << head->getMsgID() << "] std::exception:" << ex.what());
		return false;
	}
	catch (otl_exception& ex)
	{
		LOG_ERROR("handle message [" << head->getMsgID() << "] otl_exception:" << ex.msg << ",on " << ex.stm_text<<"");
		return false;
	}
	catch (...)
	{
		LOG_ERROR("catch an unknown error");
		return false;
	}
	return true;
}


void IcsClient::handleMessage(ProtocolStream& request, ProtocolStream& response)
{

	request.verify();

	switch (request.getHead()->getMsgID())
	{
	case protocol::terminal_auth_request:
		handleAuthRequest(request, response);
		break;

	case protocol::terminal_heartbeat:
		handleHeartbeat(request, response);
		break;

	default:
		LOG_ERROR("unknown message ID:" << request.getHead()->getMsgID());
		break;
	}
	
	
}


#define UPGRADE_FILE_SEGMENG_SIZE 1024

// 终端认证
void IcsClient::handleAuthRequest(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error)
{
	if (!m_conn_name.empty())
	{
		LOG_DEBUG(m_conn_name << " ignore repeat authrize message");
		response.getHead()->init(protocol::terminal_auth_response, request.getHead()->getSendNum());
		response << protocol::ShortString("ok") << (uint16_t)10;
		return;
	}

	// auth info
	string gwId, gwPwd, extendInfo;
	uint16_t deviceKind;

	request >> gwId >> gwPwd >> deviceKind >> extendInfo;

	request.assertEmpty();

	DataBase::OtlConnect conn = db.getConnection();

	if (conn == nullptr)
	{
		return;
	}

	otl_stream s(1,
		"{ call sp_authroize(:gwid<char[33],in>,:pwd<char[33],in>,@ret,@id,@name) }",
		*conn);
	
	s << gwId << gwPwd;

	otl_stream o(1, "select @ret :#<int>,@id :#<char[32]>,@name :#name<char[32]>", *conn);

	int ret = 2;
	string id, name;

	o >> ret >> id >> name;
	
	// release db connection
	db.putConnection(std::move(conn));

	response.getHead()->init(protocol::terminal_auth_response, request.getHead()->getSendNum());

	if (ret == 0)	// 成功
	{
		m_conn_name = std::move(gwId); // 保存检测点id
		response << protocol::ShortString("ok") << (uint16_t)10;
	}
	else
	{
		response << protocol::ShortString("failed");
	}
}

// 标准状态上报
void IcsClient::handleStdStatusReport(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error)
{
	uint32_t status_type;	// 标准状态类别

	request >> status_type;
	///*
	// 通用衡器
	if (status_type == 1)
	{
		uint8_t device_ligtht;	// 设备指示灯
		protocol::LongString device_status;	// 设备状态
		uint8_t cheat_ligtht;	// 作弊指示灯
		string cheat_status;	// 作弊状态
		float zero_point;		// 秤体零点	

		request >> device_ligtht>>device_status>> cheat_ligtht >> cheat_status >> zero_point;

		request.assertEmpty();

		protocol::IcsDataTime recv_time;
		protocol::getIcsNowTime(recv_time);

		DataBase::OtlConnect conn = db.getConnection();

		otl_stream o(1, 
			"{call sp_status_standard(:f<char[33],in>,:f<int,in>,:f<char[512],in>"
			",:f<int,in>,:f<char[512],in>,:f<float,in>,:f<datetieme,in>)}"
			, *conn);

		o << m_conn_name << device_ligtht << device_status << cheat_ligtht << cheat_status << zero_point << recv_time;

		db.putConnection(std::move(conn));
	}
	// 其它
	else
	{
		LOG_WARN("unknown standard status type:" << status_type);
	}
	//*/
}

// 自定义状态上报
void IcsClient::handleDefStatusReport(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error)
{
	/*

	
	MessageBuffer request(msghead + 1, msghead->length - sizeof(IcsMsgHead)-sizeof(uint32_t));	// 消息体内容

	CDatetime status_time;		//	状态采集时间
	uint16_t status_count;		//	状态项个数


	uint16_t status_id = 0;		//	状态编号
	uint8_t status_light = 0;	//	状态指示灯
	uint8_t status_type = 0;	//	状态值类型
	string status_value;		//	状态值

	request >> status_time >> status_count;

	for (uint16_t i = 0; i<status_count; i++)
	{
		request >> status_id >> status_light >> status_type >> status_value;

		// 存入数据库
		LOG4CPLUS_DEBUG("light:" << (int)status_light << ",id:" << status_id << ",type:" << (int)status_type << ",value:" << status_value);

		// store into database
		std::vector<Variant> params;
		params.push_back(terminal->m_monitorID);// 监测点编号
		params.push_back(int(status_id));		// 状态编号
		params.push_back(int(status_light));	// 状态指示灯
		params.push_back(int(status_type));		// 状态值类型
		params.push_back(status_value);			//状态值
		params.push_back(status_time);			//状态采集时间

		if (!IcsTerminal::s_core_api->CORE_CALL_SP(params, "sp_status_userDef(:F1<char[32],in>,:F2<int,in>,:F3<int,in>,:F4<int,in>,:F5<char[256],in>,:F6<timestamp,in>)",
			terminal->m_dbName, terminal->m_dbSource))
		{
			LOG4CPLUS_ERROR("execute sp sp_status_userDef error");
			return false;
		}
	}

	if (!request.empty())
	{
		LOG_WARN("report defination status message has superfluous data:" << request.leftSize());
		return false;
	}
//*/
}

// 事件上报
void IcsClient::handleEventsReport(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error)
{
	response << m_conn_name << request;

	/*

	
	MessageBuffer request(msghead + 1, msghead->length - sizeof(IcsMsgHead)-sizeof(uint32_t));	// 消息体内容

	CDatetime event_time;	//	事件发生时间
	uint16_t event_count;	// 事件项个数

	uint16_t event_id = 0;	//	事件编号
	uint8_t event_type = 0;	//	事件值类型
	string event_value;		//	事件值

	CDatetime recv_time;	// 接收时间
	getCurrentDatetime(&recv_time);


	request >> event_time >> event_count;


	// 遍历取出全部事件
	for (uint16_t i = 0; i<event_count; i++)
	{
		request >> event_id >> event_type >> event_value;

		// push message
		boost::shared_ptr<CBlock> push_package = IcsTerminal::s_core_api->CORE_CREATE_BLOCK();
		MessageBuffer push_buffer(push_package->getpointer<IcsMsgHead>(sizeof(IcsMsgHead), 256), 256);

		// 存入数据库
		std::vector<Variant> params;
		params.push_back(terminal->m_monitorID);	// 监测点编号
		params.push_back((int)terminal->m_deviceKind);
		params.push_back(int(event_id));	// 事件编号
		params.push_back(int(event_type));	// 事件值类型
		params.push_back(event_value);		// 事件值
		params.push_back(event_time);		// 事件发生时间
		params.push_back(recv_time);

		if (!IcsTerminal::s_core_api->CORE_CALL_SP(params,
			"sp_event_report(:F1<char[32],in>,:F2<int,in>,:F2<int,in>,:F4<int,in>,:F5<char[256],in>,:F6<timestamp,in>,:F7<timestamp,in>)",
			terminal->m_dbName, terminal->m_dbSource))
		{
			LOG4CPLUS_ERROR("execute sp sp_event_report error");
			return false;
		}

		// 发送给推送服务器
		push_buffer << terminal->m_monitorID << terminal->m_deviceKind << event_time << event_id << event_value;		// 监测点ID 发生时间 事件编号 事件值

		MsgPush::getInstance()->push(push_package, ICS_PROTOCOL_VERSION_11, PushMessage_Request, push_buffer.size());

	}

	if (!request.empty())
	{
		LOG_WARN("report event message has superfluous data:" << request.leftSize());
		return false;
	}
//*/
}

// 业务上报
void IcsClient::handleBusinessReport(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error)
{
	protocol::IcsDataTime report_time, recv_time;			// 业务采集时间 接收时间
	uint32_t business_no;	// 业务流水号
	uint32_t business_type;	// 业务类型

	protocol::getIcsNowTime(recv_time);

	request >> report_time >> business_no >> business_type;

	if (m_lastBusSerialNum == business_no)	// 重复的业务流水号，直接忽略
	{
		response.getHead()->init(protocol::terminal_bus_report);
		return;	
	}

	/*
	m_lastBusSerialNum = business_no;	// 更新最近的业务流水号

	if (business_type == 1)	// 静态汽车衡
	{
		protocol::ShortString cargo_num;	// 货物单号	
		protocol::ShortString vehicle_num;	// 车号
		protocol::ShortString consigness;	// 收货单位
		protocol::ShortString cargo_name;	// 货物名称

		float weight1, weight2, weight3, weight4, unit_price, money;	// 毛重 皮重 扣重 净重 单价 金额
		uint8_t in_out;	// 进出

		request >> cargo_num >> vehicle_num >> consigness >> cargo_name >> weight1 >> weight2 >> weight3 >> weight4 >> unit_price >> money >> in_out;	

		// store into database

		DataBase::OtlConnect conn = db.getConnection();

		otl_stream s(1, "", *conn);
		
		std::vector<Variant> params;
		params.push_back(terminal->m_monitorID);	// 监测点编号
		params.push_back(int(business_no));	// 业务流水号
		params.push_back(cargo_num);	// 货物单号
		params.push_back(vehicle_num);	// 车号
		params.push_back(consigness);	// 收货单位
		params.push_back(cargo_name);	// 货物名称
		params.push_back(weight1);	// 毛重
		params.push_back(weight2);	// 皮重
		params.push_back(weight3);	// 扣重
		params.push_back(weight4);	// 净重
		params.push_back(unit_price);// 单价
		params.push_back(money);	// 金额
		params.push_back(int(in_out));	// 进出
		params.push_back(report_time);		// 业务采集时间
		params.push_back(recv_time);

		if (!IcsTerminal::s_core_api->CORE_CALL_SP(params,
			"sp_business_vehicle(:F1<char[33],in>,:F3<int,in>,:F4<char[126],in>,:F5<char[126],in>,:F6<char[126],in>,:F7<char[126],in>,"
			":F8<float,in>,:F9<float,in>,:F10<float,in>,:F11<float,in>,:F12<float,in>,:F13<float,in>,:F14<int,in>,:F15<timestamp,in>,:F16<timestamp,in>)",
			"ics_vehicle", terminal->m_dbSource))
		{
			LOG4CPLUS_ERROR("execute sp sp_business_vehicle error");
			return false;
		}
	}
	else if (business_type == 2)	// 包装秤
	{
		uint8_t count;	// 秤数量
		request >> count;

		for (uint8_t i = 0; i < count; i++)	// 依次取出秤的称重数据
		{
			uint8_t number;		// 秤编号
			uint16_t amount;	// 称重次数
			float total_weight;		// 称重总重
			float single_weighet;// 单次重量

			request >> number >> amount >> total_weight >> single_weighet;

			// store into database
			std::vector<Variant> params;
			params.push_back(terminal->m_monitorID);	// 监测点编号
			params.push_back(terminal->m_monitorName);
			params.push_back((int)business_no);// 业务流水号			
			params.push_back((int)number);	// 秤编号
			params.push_back((int)amount);	// 称重次数
			params.push_back(total_weight);	// 称重总重
			params.push_back(single_weighet);	// 单次重量
			params.push_back(report_time);	// 时间
			params.push_back(recv_time);
			if (!IcsTerminal::s_core_api->CORE_CALL_SP(params, "sp_business_pack(:F1<char[33],in>,:F2<char[33],in>,:F3<int,in>,:F4<int,in>,:F5<int,in>,:F6<float,in>,:F7<float,in>,:F8<timestamp,in>,:F9<timestamp,in>)",
				"ics_packing", terminal->m_dbSource))
			{
				LOG4CPLUS_ERROR("execute sp sp_business_pack error");
				return false;
			}
		}
	}
	else if (business_type == 3)	// 公路衡器
	{
		char buff[126];
		string axle_str;	// 轴重字符串：轴重1,轴重2,轴重3 ......
		string type_str;	// 轴类型字符串：类型1,类型2,类型3 ......

		uint32_t total_weight;// 总重
		uint16_t speed;	// 车速
		uint8_t axle_num;	// 轴数(1-20)
		uint8_t type_num;	// 轴类型数(1-轴数)

		request >> total_weight >> speed >> axle_num;

		// 处理各个轴重
		for (uint8_t i = 0; i < axle_num; i++)
		{
			uint16_t axle_weight;
			request >> axle_weight;
			snprintf(buff, sizeof(buff), "%u,", axle_weight);
			axle_str += buff;
		}

		if (!axle_str.empty())
		{
			axle_str.erase(axle_str.end() - 1);
		}

		request >> type_num;

		// 处理各个轴类型
		for (uint8_t i = 0; i < type_num; i++)
		{
			uint8_t axle_type;
			request >> axle_type;
			snprintf(buff, sizeof(buff), "%u+", axle_type);
			type_str += buff;
		}

		if (!type_str.empty())
		{
			type_str.erase(type_str.end() - 1);
		}

		// store into database
		std::vector<Variant> params;
		params.push_back(terminal->m_monitorID);	// 监测点编号
		//		params.push_back(terminal->m_monitorName);
		params.push_back((int)business_no);// 业务流水号			
		params.push_back((int)total_weight);	// 总重
		params.push_back(float(speed * 0.1));	// 车速
		params.push_back((int)axle_num);	// 轴数
		params.push_back(axle_str);	// 轴重
		params.push_back((int)type_num);	// 轴类型数
		params.push_back(type_str);	// 轴类型
		params.push_back(report_time);	// 时间
		params.push_back(recv_time);
		if (!IcsTerminal::s_core_api->CORE_CALL_SP(params, "sp_business_expressway(:F1<char[33],in>,:F2<int,in>,:F3<int,in>,:F4<float,in>,:F5<int,in>,:F6<char[256],in>,:F7<int,in>,:F8<char[256],in>,:F9<timestamp,in>,:F10<timestamp,in>)",
			"ics_highway", terminal->m_dbSource))
		{
			LOG4CPLUS_ERROR("execute sp sp_business_expressway error");
			return false;
		}

	}
	else
	{
		throw logic_error("unknown business type");
	}
	request.assertEmpty();
	*/
}

// 终端回应参数查询
void IcsClient::handleParamQueryResponse(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error)
{
	/*

	
	MessageBuffer request(msghead + 1, msghead->length - sizeof(IcsMsgHead)-sizeof(uint32_t));	// 消息体内容

	uint32_t request_id;	// 请求id
	uint16_t param_count;	// 参数数量

	uint8_t net_id = 0;		//	子网编号
	uint16_t param_id = 0;	//	参数编号
	uint8_t param_type = 0;	//	参数值类型
	string param_value;		//	参数值

	request >> request_id >> param_count;

	for (uint16_t i = 0; i<param_count; i++)
	{
		request >> net_id >> param_id >> param_type >> param_value;
		// 存入数据库

	}

	if (!request.empty())
	{
		LOG_WARN("reply query paramter message has superfluous data:" << request.leftSize());
		return false;
	}

//*/
}

// 终端主动上报参数修改
void IcsClient::handleParamAlertReport(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error)
{
	/*

	
	MessageBuffer request(msghead + 1, msghead->length - sizeof(IcsMsgHead)-sizeof(uint32_t));	// 消息体内容

	CDatetime alert_time;	//	修改时间
	uint16_t param_count;	// 参数数量

	uint8_t net_id = 0;		//	子网编号
	uint16_t param_id = 0;	//	参数编号
	uint8_t param_type = 0;	//	参数值类型
	string param_value;		//	参数值

	request >> alert_time >> param_count;

	for (uint16_t i = 0; i<param_count; i++)
	{
		request >> net_id >> param_id >> param_type >> param_value;

		// 存入数据库

	}

	if (!request.empty())
	{
		LOG_WARN("report alert paramter message has superfluous data:" << request.leftSize());
		return false;
	}
//*/
}

// 终端回应参数修改
void IcsClient::handleParamModifyResponse(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error)
{
	/*

	
	MessageBuffer request(msghead + 1, msghead->length - sizeof(IcsMsgHead)-sizeof(uint32_t));	// 消息体内容

	uint32_t request_id;	// 请求id
	uint16_t param_count;	// 参数数量

	uint8_t net_id = 0;		//	子网编号
	uint16_t param_id = 0;	//	参数编号
	string alert_value;		//	修改结果

	request >> request_id >> param_count;

	for (uint16_t i = 0; i<param_count; i++)
	{
		request >> net_id >> param_id >> alert_value;
		// 存入数据库

	}

	if (!request.empty())
	{
		LOG_WARN("reply modify paramter message has superfluous data:" << request.leftSize());
		return false;
	}
//*/
}

// 终端发送时钟同步请求
void IcsClient::handleDatetimeSync(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error)
{
	protocol::IcsDataTime dt1, dt2;
	request >> dt1;
	request.assertEmpty();

	protocol::getIcsNowTime(dt2);

	response.getHead()->init(protocol::terminal_datetime_sync_response, m_send_num++);
	response << dt1 << dt2 << dt2;
}

// 终端上报日志
void IcsClient::handleLogReport(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error)
{
	/*

	
	MessageBuffer request(msghead + 1, msghead->length - sizeof(IcsMsgHead)-sizeof(uint32_t));	// 消息体内容

	CDatetime status_time;		//	时间
	uint8_t log_level;			//	日志级别
	uint8_t encode_type = 0;	//	编码方式
	string log_value;			//	日志内容

	request >> status_time >> log_level >> encode_type >> log_value;

	if (encode_type == 1)	// 0-UTF-8,1-GB2312
	{
		if (!IcsTerminal::charsetConv("GB2312", "UTF-8", log_value))
		{
			LOG4CPLUS_ERROR("编码转换失败");
			return false;
		}
	}
	else if (encode_type != 0)
	{
		LOG4CPLUS_ERROR("unkonw encode type: " << (int)encode_type);
		return false;
	}

	// store into database
	std::vector<Variant> params;
	params.push_back(terminal->m_monitorID);// 监测点编号
	params.push_back(status_time);			// 时间
	params.push_back(int(log_level));		// 日志级别	
	params.push_back(log_value);			// 日志内容


	if (!IcsTerminal::s_core_api->CORE_CALL_SP(params, "sp_log_report(:F1<char[32],in>,:F2<timestamp,in>,:F3<int,in>,:F4<char[256],in>)",
		terminal->m_dbName, terminal->m_dbSource))
	{
		LOG4CPLUS_ERROR("execute sp sp_log_report error");
		return false;
	}


	if (!request.empty())
	{
		LOG_WARN("report log message has superfluous data:" << request.leftSize());
		return false;
	}
//*/
}

// 终端发送心跳到中心
void IcsClient::handleHeartbeat(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error)
{

}

// 终端拒绝升级请求
void IcsClient::handleDenyUpgrade(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error)
{
	/*

	MessageBuffer request(msghead + 1, msghead->length - sizeof(IcsMsgHead)-sizeof(uint32_t));	// 消息体内容

	uint32_t request_id;	// 请求id
	string reason;	// 拒绝升级原因

	request >> request_id >> reason;

	// verify length
	if (!request.empty())
	{
		LOG_WARN("reply deny upgrade message has superfluous data:" << request.leftSize());
		return false;
	}

	vector<Variant> params;
	params.push_back(terminal->m_monitorID);
	params.push_back(int(request_id));
	params.push_back(reason);

	if (!IcsTerminal::s_core_api->CORE_CALL_SP(params,
		"sp_upgrade_refuse(:F1<char[33],in>,:F2<int,in>,:F3<char[126],in>)",
		terminal->m_dbName, terminal->m_dbSource))
	{
		LOG4CPLUS_ERROR("execute sp sp_upgrade_refuse error");
		return false;
	}

	//*/

}

// 终端接收升级请求
void IcsClient::handleAgreeUpgrade(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error)
{
	/*

	
	MessageBuffer request(msghead + 1, msghead->length - sizeof(IcsMsgHead)-sizeof(uint32_t));	// 消息体内容

	uint32_t request_id;	// 文件id

	request >> request_id;

	if (!request.empty())
	{
		LOG4CPLUS_ERROR("reply agree upgrade message has superfluous data:" << request.leftSize());
		return false;
	}

	vector<Variant> params;
	params.push_back(terminal->m_monitorID);
	params.push_back(int(request_id));

	if (!IcsTerminal::s_core_api->CORE_CALL_SP(params,
		"sp_upgrade_accept(:F1<char[33],in>,:F2<int,in>)",
		terminal->m_dbName, terminal->m_dbSource))
	{
		LOG4CPLUS_ERROR("execute sp sp_upgrade_accept error");
		return false;
	}
//*/
}

// 索要升级文件片段
void IcsClient::handleRequestFile(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error)
{
	/*

	
	MessageBuffer request(msghead + 1, msghead->length - sizeof(IcsMsgHead)-sizeof(uint32_t));	// 消息体内容

	uint32_t file_id, request_id, fragment_offset, received_size;

	uint16_t fragment_length;

	request >> file_id >> request_id >> fragment_offset >> fragment_length >> received_size;

	if (!request.empty())
	{
		LOG_WARN("request upgrade file message has superfluous data:" << request.leftSize());
		return false;
	}

	// 
	vector<Variant> params;

	// 设置升级进度(查询该请求id对应的状态)
	params.clear();
	params.push_back(int(request_id));
	params.push_back((int)received_size);
	params.push_back(1);
	if (!IcsTerminal::s_core_api->CORE_CALL_SP(params,
		"sp_upgrade_set_progress(:F1<int,in>,:F2<int,in>,:G1<int,out>)",
		terminal->m_dbName, terminal->m_dbSource))
	{
		LOG4CPLUS_ERROR("execute sp sp_upgrade_set_progress error");
		return false;
	}

	// 查找文件
	const FileUpgradeManager::FileInfo* fileInfo = FileUpgradeManager::getInstance()->getFileInfo(file_id);
	// reply message
	boost::shared_ptr<CBlock> sendPackage = IcsTerminal::s_core_api->CORE_CREATE_BLOCK();
	MessageBuffer input_buf(sendPackage->getpointer<char>(sizeof(IcsMsgHead), 64), UPGRADE_FILE_SEGMENG_SIZE + sizeof(IcsMsgHead)+sizeof(uint32_t)* 5);


	// 查看是否已取消升级
	if (boost::get<int>(params[2]) == 0 && fileInfo)	// 正常状态且找到该文件
	{
		if (fragment_offset > fileInfo->file_length)	// 超出文件大小
		{
			LOG_WARN("require file offset (" << fragment_offset << ") is more than file's length (" << fileInfo->file_length << ")");
			return false;
		}
		else if (fragment_offset + fragment_length > fileInfo->file_length)	// 超过文件大小则取剩余大小
		{
			fragment_length = fileInfo->file_length - fragment_offset;
		}

		if (fragment_length > UPGRADE_FILE_SEGMENG_SIZE)	// 限制文件片段最大为缓冲区剩余长度
		{
			fragment_length = UPGRADE_FILE_SEGMENG_SIZE;
		}

		input_buf << file_id << request_id << fragment_offset << fragment_length;
		input_buf.append((char*)fileInfo->file_content + fragment_offset, fragment_length);

		// send
		terminal->sendBlockdata(sendPackage, ICS_PROTOCOL_VERSION_11, IcsMsg_upgrade_file_response, input_buf.size());

	}
	else	// 无升级事务
	{
		// send
		terminal->sendBlockdata(sendPackage, ICS_PROTOCOL_VERSION_11, IcsMsg_upgrade_not_found, input_buf.size());

	}
//*/
}

// 升级文件传输结果
void IcsClient::handleUpgradeResult(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error)
{
	/*

	
	MessageBuffer request(msghead + 1, msghead->length - sizeof(IcsMsgHead)-sizeof(uint32_t));	// 消息体内容

	uint32_t request_id;	// 文件id
	string upgrade_result;	// 升级结果

	request >> request_id >> upgrade_result;

	if (!request.empty())
	{
		LOG_WARN("report upgrade result message has superfluous data:" << request.leftSize());
		return false;
	}

	vector<Variant> params;
	params.push_back(int(request_id));
	params.push_back(upgrade_result);

	if (!IcsTerminal::s_core_api->CORE_CALL_SP(params,
		"sp_upgrade_result(:F1<int,in>,:F3<char[126],in>)",
		terminal->m_dbName, terminal->m_dbSource))
	{
		LOG4CPLUS_ERROR("execute sp sp_upgrade_result error");
		return false;
	}
//*/

}

// 终端确认取消升级
void IcsClient::handleUpgradeCancelAck(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error)
{
	/*

	
	MessageBuffer request(msghead + 1, msghead->length - sizeof(IcsMsgHead)-sizeof(uint32_t));	// 消息体内容

	uint32_t request_id;	// 文件id

	request >> request_id;
	if (!request.empty())
	{
		LOG_WARN("report upgrade cancel ack message has superfluous data:" << request.leftSize());
		return false;
	}

	vector<Variant> params;
	params.push_back(int(request_id));

	if (!IcsTerminal::s_core_api->CORE_CALL_SP(params,
		"sp_upgrade_cancel_ack(:F1<int,in>)",
		terminal->m_dbName, terminal->m_dbSource))
	{
		LOG4CPLUS_ERROR("execute sp sp_upgrade_cancel_ack error");
		return false;
	}
//*/
}

//----------------------------------------------------------------------------------//



}// end namespace ics
