
#include "config.hpp"
#include "icsclient.hpp"
#include "log.hpp"
#include "database.hpp"
#include <cstdio>

extern ics::DataBase db;
extern ics::MemoryPool mp;

namespace ics{

//---------------------------------IcsConnection-------------------------------------------------//
    
IcsConnection::IcsConnection(asio::ip::tcp::socket&& s, ClientManager& cm)
: TcpConnection(std::forward<asio::ip::tcp::socket>(s)), m_client_manager(cm), m_isSending(false)
{
    
}

IcsConnection::~IcsConnection()
{
	if (m_clientHandler != nullptr)
	{
		delete m_clientHandler;
	}
	LOG_DEBUG((m_clientHandler != nullptr ? m_clientHandler->name() : m_connectionID) << " has been deleted");
}
   
void IcsConnection::start()
{
	do_read();
//	do_write();	// nothing to write
}

void IcsConnection::do_read()
{
	// wait response
	m_socket.async_read_some(asio::buffer(m_recv_buf, sizeof(m_recv_buf)),
		[this](const std::error_code& ec, std::size_t length)
		{
			// no error and handle message
			if (!ec)
			{
				if (handleData(m_recv_buf, length))
				{
					// continue to read		
					do_read();
				}			
			}
			else
			{
				LOG_DEBUG(m_conn_name << " recv data error: "<<ec.message());
				do_error();
			}
		});
}

void IcsConnection::do_write()
{
	trySend();
}

void IcsConnection::toHexInfo(const char* info, uint8_t* data, std::size_t length)
{
#ifndef NDEBUG
	char buff[1024];
	for (size_t i = 0; i < length && i < sizeof(buff)/3; i++)
	{
		std::sprintf(buff + i * 3, " %02x", data[i]);
	}
//	LOG_DEBUG(m_conn_name << info << length << " bytes:" << buff);
	LOG_DEBUG(m_conn_name << " " << info << " " << length << " bytes...");
#endif
}

void IcsConnection::trySend()
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
					MemoryChunk& mc = m_sendList.front();
					this->toHexInfo("send", (uint8_t*)mc.getBuff(), mc.getUsedSize());
					mp.put(mc);
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

void IcsConnection::trySend(MemoryChunk& mc)
{
//	std::lock_guard<std::mutex> lock(m_sendLock);
	m_sendList.push_back(mc);
	trySend();
}

void IcsConnection::replyResponse(uint16_t ackNum)
{
	MemoryChunk mc = mp.get();

	ProtocolStream response(mc.getBuff(), mc.getTotalSize());
	
	response.getHead()->init(1, ackNum);

	response.getHead()->setSendNum(m_send_num++);

	response.serailzeToData();
	mc.setUsedSize(response.length());
	trySend(mc);
}

bool IcsConnection::handleData(uint8_t* buf, std::size_t length)
{
	// show debug info
	this->toHexInfo("recv", buf, length);

	if (m_clientHandler == nullptr)
	{
		// 
		m_clientHandler = new TerminalClient(*this);
	}
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
	ProtocolHead* head = request.getHead();

	if (head->isResponse())	// ignore response message from terminal
	{
		return true;
	}


	MemoryChunk mc = mp.get();

	ProtocolStream response(mc.getBuff(), mc.getTotalSize());

	try {
		m_clientHandler->handle(request, response);

		if (head->needResposne())
		{
			response.getHead()->init(head->getMsgID(), head->getSendNum());
		}

		if (!response.empty() || head->needResposne())
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
		LOG_ERROR("handle message [" << head->getMsgID() << "] otl_exception:" << ex.msg);
		return false;
	}
	catch (...)
	{
		LOG_ERROR("catch an unknown error");
		return false;
	}
	return true;
}


//---------------------------------TerminalClient-------------------------------------------------//

void TerminalClient::handleFromTerminal(ProtocolStream& request, ProtocolStream& response)
{
	// verify the message
	request.verify();

#if defined(ICS_CENTER_MODE)
	request >> m_conn_name;
#else

#endif

	switch (request.getHead()->getMsgID())
	{
	case protocol::terminal_auth_request:
		handleAuthRequest(request, response);
		break;

	case protocol::terminal_heartbeat:
		handleHeartbeat(request, response);
		break;

	case protocol::terminal_std_status_report:
		handleStdStatusReport(request, response);
		break;

	case protocol::terminal_def_status_report:
		handleDefStatusReport(request, response);
		break;

	case protocol::terminal_event_report:
		handleEventsReport(request, response);
		break;

	case protocol::terminal_bus_report:
		handleBusinessReport(request, response);
		break;

	case protocol::terminal_gps_report:
		handleGpsReport(request, response);
		break;

	case protocol::terminal_datetime_sync_request:
		handleDatetimeSync(request, response);
		break;

	case protocol::terminal_log_report:
		handleLogReport(request, response);
		break;

	default:
		LOG_ERROR("unknown message ID:" << request.getHead()->getMsgID());
		throw std::runtime_error("unknown message id");
		break;
	}
}

#define UPGRADE_FILE_SEGMENG_SIZE 1024

// 终端认证
void TerminalClient::handleAuthRequest(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error)
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
	
	request >> gwId >> gwPwd >> m_deviceKind >> extendInfo;

	request.assertEmpty();

	DataBase::OtlConnect conn = db.getConnection();

	if (conn == nullptr)
	{
		return;
	}

	otl_stream authroizeStream(1,
		"{ call sp_authroize(:gwid<char[33],in>,:pwd<char[33],in>,@ret,@id,@name) }",
		*conn);
	
	authroizeStream << gwId << gwPwd;
	authroizeStream.close();

	otl_stream getStream(1, "select @ret :#<int>,@id :#<char[32]>,@name :#name<char[32]>", *conn);

	int ret = 2;
	string id, name;

	getStream >> ret >> id >> name;
	getStream.close();

	response.getHead()->init(protocol::terminal_auth_response, request.getHead()->getSendNum());

	if (ret == 0)	// 成功
	{
		m_conn_name = std::move(gwId); // 保存检测点id
		otl_stream onlineStream(1, "{ call sp_online(:id<char[33],in>,:ip<char[16],in>,:port<int,in>) }", *conn);

		onlineStream << m_conn_name << "127.0.0.1" << 9999;
		onlineStream.close();
		response << protocol::ShortString("ok") << (uint16_t)10;
	}
	else
	{
		response << protocol::ShortString("failed");
	}
	// release db connection
	db.putConnection(std::move(conn));

}

// 标准状态上报
void TerminalClient::handleStdStatusReport(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error)
{
	uint32_t status_type;	// 标准状态类别

	request >> status_type;

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
			"{ call sp_status_standard(:id<char[33],in>,:devFlag<int,in>,:devStat<char[512],in>,:cheatFlag<int,in>,:cheatStat<char[512],in>,:zeroPoint<float,in>,:recvTime<timestamp,in>) }"
			, *conn);

		o << m_conn_name << (int)device_ligtht << device_status << (int)cheat_ligtht << cheat_status << zero_point << recv_time;

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
void TerminalClient::handleDefStatusReport(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error)
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
			LOG_ERROR("execute sp sp_status_userDef error");
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
void TerminalClient::handleEventsReport(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error)
{
	protocol::IcsDataTime event_time, recv_time;	//	事件发生时间 接收时间
	uint16_t event_count;	// 事件项个数

	uint16_t event_id = 0;	//	事件编号
	uint8_t event_type = 0;	//	事件值类型
	string event_value;		//	事件值

	protocol::getIcsNowTime(recv_time);

	request >> event_time >> event_count;


	DataBase::OtlConnect conn = db.getConnection();
	otl_stream eventStream(1, "{ call sp_event_report(:id<char[33],in>,:devKind<int,in>,:eventID<int,in>,:eventType<int,in>,:eventValue<char[256],in>,:eventTime<timestamp,in>,:recvTime<timestamp,in>) }", *conn);


	// 遍历取出全部事件
	for (uint16_t i = 0; i<event_count; i++)
	{
		request >> event_id >> event_type >> event_value;

		eventStream << m_conn_name << (int)m_deviceKind << (int)event_id << (int)event_type << event_value << event_time << recv_time;

		// 发送给推送服务器
//		push_buffer << terminal->m_monitorID << terminal->m_deviceKind << event_time << event_id << event_value;		// 监测点ID 发生时间 事件编号 事件值

//		MsgPush::getInstance()->push(push_package, ICS_PROTOCOL_VERSION_11, PushMessage_Request, push_buffer.size());

	}
	db.putConnection(std::move(conn));
	request.assertEmpty();
}

// 业务上报
void TerminalClient::handleBusinessReport(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error)
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

	DataBase::OtlConnect conn = db.getConnection();

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

		otl_stream s(1
			, "{ call `ics_vehicle`.sp_business_vehicle(:id<char[33],in>,:num<int,in>,:cargoNum<char[126],in>,:vehNum<char[126],in>"
				",:consigness<char[126],in>,:cargoName<char[126],in>,:weight1<float,in>,:weight2<float,in>,:weight3<float,in>,:weight4<float,in>"
				",:unitPrice<float,in>,:money<float,in>,:inOrOut<int,in>,:reportTime<timestamp,in>,:recvTime<timestamp,in>) }"
			, *conn);
		
		s << m_conn_name <<(int)business_no << cargo_num << vehicle_num
			<< consigness << cargo_name << weight1 << weight2 << weight3 << weight4
			<< unit_price << money << (int)in_out << report_time << recv_time;
		
	}
	else if (business_type == 2)	// 包装秤
	{
		uint8_t count;	// 秤数量
		request >> count;

		otl_stream s(1, 
			"{ call `ics_packing`.sp_business_pack(:id<char[33],in>,:num<int,in>,:amount<int,in>,:weight<int,in>,:sWeight<int,in>,:F6<float,in>,:reportTime<timestamp,in>,:recvTime<timestamp,in>) }", *conn);

		for (uint8_t i = 0; i < count; i++)	// 依次取出秤的称重数据
		{
			uint8_t number;		// 秤编号
			uint16_t amount;	// 称重次数
			float total_weight;		// 称重总重
			float single_weighet;// 单次重量

			request >> number >> amount >> total_weight >> single_weighet;
			
			s << m_conn_name << business_no << (int)amount << total_weight << single_weighet << report_time << recv_time;
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
			std::sprintf(buff, "%u,", axle_weight);
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
			std::sprintf(buff, "%u+", axle_type);
			type_str += buff;
		}

		if (!type_str.empty())
		{
			type_str.erase(type_str.end() - 1);
		}

		otl_stream s(1,
			"{ call `ics_highway`.sp_business_expressway(:id<char[33],in>,:num<int,in>,:weight<int,in>,:speed<float,in>,:axleNum<int,in>,:axleStr<char[256],in>,:typeNum<int,in>,:typeStr<char[256],in>,:reportTime<timestamp,in>,:recvTime<timestamp,in>) }"
			, *conn);

		s << m_conn_name << (int)business_no << (int)total_weight << speed*0.1 << (int)axle_num << axle_str << (int)type_num << type_str << report_time << recv_time;
	}
	else if (business_type == 4)	// 餐厨车
	{
		union
		{
			uint8_t	data;
			struct {
				uint8_t mode : 1;	// 工作模式，0-手动模式，1-自动模式
				uint8_t unit : 1;	// 重量单位，0-千克，1-磅
				uint8_t card : 1;	// 刷卡状态，0-用户刷卡，1-司机刷卡
				uint8_t flow : 1;	// 流程状态，0-流程正常，1-流程异常
				uint8_t evalution : 2;	// 评价，0-好评，1-中评，2-差评
				uint8_t reserved : 2;
			};
		}weightFlag;

		protocol::ShortString tubID;
		uint32_t tubVolumn, weight, driverID;

		request >> weightFlag.data >> tubID >> tubVolumn >> weight >> driverID;

		union
		{
			uint8_t	data;
			struct {
				uint8_t longitude_flag : 1;	// 经度符号，0-东经，1-西经
				uint8_t latitude_flag : 1;	// 纬度符号，0-南纬，1-北纬
				uint8_t signal : 3;	// GPS信号强度，0-很强，1-较强，2-一般，3-较弱，4-无
				uint8_t reserved : 3;
			};
		}postionFlag;
		uint32_t longitude, latitude, height, speed;

		request >> postionFlag.data >> longitude >> latitude >> height >> speed;

		otl_stream s(1
			, "{ call `ics_canchu`.sp_weight_report(:id<char[33],in>,:num<int,in>,:reportTime<timestamp,in>,:recvTime<timestamp,in>"
			",:mode<int,in>,:unit<int,in>,:cardid<int,in>,:flow<int,in>,:evalution<int,in>"
			",:tubID<char[33],in>,:volumn<int,in>,:weight<int,in>,:driverID<int,in>"
			",:logFlag<int,in>,:logitude<int,in>,:laFlag<int,in>,:latitude<int,in>,:signal<int,in>,:height<int,in>,:speed<int,in>) }"
			, *conn);

		s << m_conn_name << (int)business_no << report_time << recv_time
			<< (int)weightFlag.mode << (int)weightFlag.unit << (int)weightFlag.card << (int)weightFlag.flow << (int)weightFlag.evalution
			<< tubID << (int)tubVolumn << (int)weight << (int)driverID
			<< (int)postionFlag.longitude_flag << (int)longitude << (int)postionFlag.latitude_flag << (int)latitude << (int)postionFlag.signal << (int)height << (int)speed;
	}
	else
	{
		throw logic_error("unknown business type");
	}

	request.assertEmpty();
	
}

// GPS上报
void TerminalClient::handleGpsReport(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error)
{
	union
	{
		uint8_t	data;
		struct {
			uint8_t longitude_flag : 1;	// 经度符号，0-东经，1-西经
			uint8_t latitude_flag : 1;	// 纬度符号，0-南纬，1-北纬
			uint8_t signal : 3;	// GPS信号强度，0-很强，1-较强，2-一般，3-较弱，4-无
			uint8_t reserved : 3;
		};
	}postionFlag;

	uint32_t longitude, latitude, height, speed;

	request >> postionFlag.data >> longitude >> latitude >> height >> speed;

	request.assertEmpty();

	DataBase::OtlConnect conn = db.getConnection();

	otl_stream s(1
		, "{ call `ics_canchu`.sp_gps_report(:id<char[33],in>,:logFlag<int,in>,:logitude<int,in>,:laFlag<int,in>,:latitude<int,in>,:signal<int,in>,:height<int,in>,:speed<int,in>) }"
		, *conn);

	s << m_conn_name << (int)postionFlag.longitude_flag << (int)longitude << (int)postionFlag.latitude_flag << (int)latitude << (int)postionFlag.signal << (int)height << (int)speed;
}

// 终端回应参数查询
void TerminalClient::handleParamQueryResponse(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error)
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
void TerminalClient::handleParamAlertReport(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error)
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
void TerminalClient::handleParamModifyResponse(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error)
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
void TerminalClient::handleDatetimeSync(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error)
{
	protocol::IcsDataTime dt1, dt2;
	request >> dt1;
	request.assertEmpty();

	protocol::getIcsNowTime(dt2);

	response.getHead()->init(protocol::terminal_datetime_sync_response, m_send_num++);
	response << dt1 << dt2 << dt2;
}

// 终端上报日志
void TerminalClient::handleLogReport(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error)
{
	protocol::IcsDataTime status_time;		//	时间
	uint8_t log_level;			//	日志级别
	uint8_t encode_type = 0;	//	编码方式
	string log_value;			//	日志内容

	request >> status_time >> log_level >> encode_type >> log_value;

	request.assertEmpty();

	if (encode_type == 1)	// 0-UTF-8,1-GB2312
	{
//		if (!IcsTerminal::charsetConv("GB2312", "UTF-8", log_value))
		{
			LOG_ERROR("编码转换失败");
			throw std::runtime_error("convert character failed");
		}
	}
	else if (encode_type != 0)
	{
		LOG_ERROR("unkonwn encode type: " << (int)encode_type);
		throw std::runtime_error("undefined encode type");
	}

	DataBase::OtlConnect conn = db.getConnection();
	otl_stream s(1, "{ call sp_log_report(:id<char[32],in>,:logtime<timestamp,in>,:logLevel<int,in>,:logValue<char[256],in>) }", *conn);

	s << m_conn_name << status_time << (int)log_level << log_value;

	db.putConnection(std::move(conn));

}

// 终端发送心跳到中心
void TerminalClient::handleHeartbeat(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error)
{

}

// 终端拒绝升级请求
void TerminalClient::handleDenyUpgrade(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error)
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
		LOG_ERROR("execute sp sp_upgrade_refuse error");
		return false;
	}

	//*/

}

// 终端接收升级请求
void TerminalClient::handleAgreeUpgrade(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error)
{
	/*

	
	MessageBuffer request(msghead + 1, msghead->length - sizeof(IcsMsgHead)-sizeof(uint32_t));	// 消息体内容

	uint32_t request_id;	// 文件id

	request >> request_id;

	if (!request.empty())
	{
		LOG_ERROR("reply agree upgrade message has superfluous data:" << request.leftSize());
		return false;
	}

	vector<Variant> params;
	params.push_back(terminal->m_monitorID);
	params.push_back(int(request_id));

	if (!IcsTerminal::s_core_api->CORE_CALL_SP(params,
		"sp_upgrade_accept(:F1<char[33],in>,:F2<int,in>)",
		terminal->m_dbName, terminal->m_dbSource))
	{
		LOG_ERROR("execute sp sp_upgrade_accept error");
		return false;
	}
//*/
}

// 索要升级文件片段
void TerminalClient::handleRequestFile(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error)
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
		LOG_ERROR("execute sp sp_upgrade_set_progress error");
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
void TerminalClient::handleUpgradeResult(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error)
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
		LOG_ERROR("execute sp sp_upgrade_result error");
		return false;
	}
//*/

}

// 终端确认取消升级
void TerminalClient::handleUpgradeCancelAck(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error)
{

	

	

	uint32_t request_id;	// 文件id

	request >> request_id;

	/*
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
		LOG_ERROR("execute sp sp_upgrade_cancel_ack error");
		return false;
	}
//*/
}

//---------------------------------WebClient-------------------------------------------------//



}// end namespace ics
