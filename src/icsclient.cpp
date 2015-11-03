
#include "config.hpp"
#include "icsclient.hpp"
#include "icsprotocol.hpp"
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
: TcpConnection(std::forward<asio::ip::tcp::socket>(s), cm), m_ics_protocol(m_recv_buf, sizeof(m_recv_buf))
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
			if (!ec && do_handle_msg(m_recv_buf, length))
			{
				// continue to read		
				do_read();
			}
			else
			{
				LOG_DEBUG(m_conn_name << " recv or handle data error");
				do_error();
			}
		});
}

void IcsClient::do_write()
{
	std::lock_guard<std::mutex> lock(m_sendLock);
	if (!m_send_list.empty())
	{
		MemoryChunk& chunk = m_send_list.front();
		m_socket.async_send(asio::buffer(chunk.getBuff(), chunk.getUsedSize()),
			[this](const std::error_code& ec, std::size_t length)
		{
			if (!ec)
			{
				m_send_list.pop_front();
				do_write();
			}
			else
			{
				LOG_DEBUG(m_conn_name << " send data error");
				do_error();
			}
		});
	}
	
}

void IcsClient::sendData(MemoryChunk& chunk)
{
	m_send_list.push_back(chunk);
	do_write();
}

bool IcsClient::do_handle_msg(uint8_t* buf, size_t length)
{
	this->debug_msg(buf, length);



	m_ics_protocol.reset(buf, length);

	IcsProtocol::IcsMsgHead* head = m_ics_protocol.getHead();

	if (m_conn_name.empty())
	{
		if (length < sizeof(IcsProtocol::IcsMsgHead) + sizeof(uint16_t))
		{
			LOG_ERROR("first package's size is not more than IcsMsgHead");
			return false;
		}

		if (head->getMsgID() != IcsProtocol::terminal_auth_request)
		{
			LOG_ERROR("first package isn't authrize message");
			return false;
		}
	}


	try {
		m_ics_protocol.verify();

		switch (head->getMsgID())
		{
		case IcsProtocol::terminal_auth_request:
			handleAuthRequest(m_ics_protocol);
			break;

		case IcsProtocol::terminal_heartbeat:
			handleHeartbeat(m_ics_protocol);
			break;

		default:
			LOG_ERROR("unknown message ID:" << head->getMsgID());
			break;
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

void IcsClient::debug_msg(uint8_t* data, size_t length)
{
#ifndef NDEBUG
	char buff[1024];
	for (size_t i = 0; i < length && i < sizeof(buff)/3; i++)
	{
		std::sprintf(buff + i * 3, "%02x ", data[i]);
	}
	LOG_DEBUG(m_conn_name << " recv " << length << " bytes: " << buff);
#endif
}

void IcsClient::do_authrize(IcsProtocol& proto) throw(std::logic_error)
{
	DataBase::OtlConnect conn = db.getConnection();

	try {
		int id = 13;
		otl_stream s(1, "{call sp_test(:f1<int,in>)}", *conn);
//		otl_stream s(1, "select * from t_class where id < :f1<int>", *conn);
		s << id;

		
		char name[64];
		while (!s.eof())
		{
			s >> id >> name;
			cout << "id:" << id << ",name:" << name << endl;
		}
	}
	catch (otl_exception& ex)
	{
		LOG_DEBUG("exec database error:" << ex.msg);
	}
	db.putConnection(std::move(conn));
}

#define UPGRADE_FILE_SEGMENG_SIZE 1024

/*
void IcsClient::handleMsgBody(IcsProtocol& proto) throw(std::logic_error)
{
	bool ret = false;
	std::map<uint16_t, handler>::iterator it = m_handlerMap.find(msghead->id);

	if (terminal->m_monitorID.empty() && msghead->id != IcsMsg_auth_request)	// 该终端未认证前不能发送其它消息
	{
		LOG4CPLUS_ERROR(IcsTerminal::s_logger, "Not allowed to send message before auth " << msghead->id);
	}
	else if (it != m_handlerMap.end())
	{
		try
		{
			ret = (*it->second)(terminal, msghead);

			// 根据需要回复响应消息
			if (msghead->flag.option.response == ICS_ATTR_NEEDRESPONSE)
			{
				terminal->sendResponseMsg(ICS_PROTOCOL_VERSION_11, msghead->send_num);
			}

		}
		catch (const std::runtime_error& ex)
		{
			LOG4CPLUS_ERROR(IcsTerminal::s_logger, "hanle message error,id:" << msghead->id << ",execpion:" << ex.what());
		}
		catch (...)
		{
			LOG4CPLUS_ERROR(IcsTerminal::s_logger, "catch an unkown execpt while handle message:" << msghead->id);
		}
	}
	else
	{
		LOG4CPLUS_ERROR(IcsTerminal::s_logger, "unknown message id:" << msghead->id);
	}

	return ret;
}
*/

/*
void IcsClient::handleMsg(IcsTerminal* terminal, uint8_t* buf, uint16_t length)
{
	IcsMsgHead* msghead = (IcsMsgHead*)buf;

	MessageBuffer mbuffer(buf + sizeof(IcsMsgHead), msghead->length - sizeof(IcsMsgHead)-sizeof(uint32_t));	// 消息体内容

	string monitor_id;
	uint16_t msg_id = 0;		// 消息id 

	mbuffer >> monitor_id >> msg_id;	// 跳过这两个字段

	// reply message
	boost::shared_ptr<CBlock> sendPackage = IcsTerminal::s_core_api->CORE_CREATE_BLOCK();
	MessageBuffer input_buf(sendPackage->getpointer<char>(sizeof(IcsMsgHead), 64), 126);

	// 剩余消息体内容复制到发送块
	input_buf.append(mbuffer, mbuffer.leftSize());

	terminal->sendBlockdata(sendPackage, ICS_PROTOCOL_VERSION_11, msg_id, input_buf.size());

	return true;
}
*/

/*
void IcsClient::handleError(IcsTerminal* terminal)
{
	if (!terminal->m_monitorID.empty())
	{
		std::vector<Variant> params;
		params.push_back(terminal->m_monitorID);
		params.push_back(IcsTerminal::s_core_api->CORE_GET_ATTR("icsmodule", "conn_addr"));
		params.push_back(boost::lexical_cast<int32_t>(IcsTerminal::s_core_api->CORE_GET_ATTR("icsmodule", "conn_port")));
		if (!IcsTerminal::s_core_api->CORE_CALL_SP(params, "sp_offline(:F1<char[33],in>,:F2<char[33],in>,:F3<int,in>)", terminal->m_dbName, terminal->m_dbSource))
		{
			LOG4CPLUS_ERROR(IcsTerminal::s_logger, "execute sp sp_offline error");
			return false;
		}

		LOG4CPLUS_DEBUG(IcsTerminal::s_logger, terminal->m_monitorID << " disconnect from server");
	}

	// 发送推送消息
	if (terminal->m_isReplaced)
	{
		
		// push message
//		boost::shared_ptr<CBlock> push_package = IcsTerminal::s_core_api->CORE_CREATE_BLOCK();
//		MessageBuffer push_buffer(push_package->getpointer<IcsMsgHead>(sizeof(IcsMsgHead), 256), 256);

		// 发送给推送服务器
//		push_buffer << terminal->m_monitorID << terminal->m_deviceKind << event_time << event_id << event_value;		// 监测点ID 发生时间 事件编号 事件值

//		MsgPush::getInstance()->push(push_package, ICS_PROTOCOL_VERSION_11, PushMessage_Request, push_buffer.size());
		
	}

	return true;
}
*/

// 终端认证
void IcsClient::handleAuthRequest(IcsProtocol& proto) throw(std::logic_error)
{
	if (!m_conn_name.empty())
	{
		LOG_DEBUG(m_conn_name << " ignore repeat authrize message");
		return;
	}

	// auth info
	string gwId, gwPwd, extendInfo;
	uint16_t deviceKind;

	proto >> gwId >> gwPwd >> deviceKind >> extendInfo;

	proto.assertEmpty();

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

	MemoryChunk chunk = mp.get();

	if (!chunk.valid())
	{
		LOG_WARN("cann't get memory chunk");
		return;
	}

	IcsProtocol outProto(chunk.getBuff(), chunk.getTotalSize());
	outProto.initHead(IcsProtocol::terminal_auth_response, m_send_num);

	if (ret == 0)	// 成功
	{
		m_conn_name = std::move(gwId); // 保存检测点id
		outProto << "ok" << (uint16_t)10;	
	}
	else
	{
		outProto << "failed";
	}

	outProto.serailzeToData();

	chunk.setUsedSize(outProto.msgLength());

	this->sendData(chunk);

	/*

	// reply auth message
	
		params.clear();
		params.push_back(terminal->m_monitorID);
		params.push_back(IcsTerminal::s_core_api->CORE_GET_ATTR("icsmodule", "conn_addr"));
		params.push_back(boost::lexical_cast<int32_t>(IcsTerminal::s_core_api->CORE_GET_ATTR("icsmodule", "conn_port")));
		if (!IcsTerminal::s_core_api->CORE_CALL_SP(params, "sp_online(:F1<char[33],in>,:F2<char[33],in>,:F3<int,in>)", terminal->m_dbName, terminal->m_dbSource))
		{
			LOG4CPLUS_ERROR(IcsTerminal::s_logger, "execute sp sp_online error");
			return false;
		}

		input_buf << "ok" << boost::lexical_cast<uint16_t>(IcsTerminal::s_core_api->CORE_GET_ATTR("icsmodule", "conn_timeout"));
	}
	else // 认证失败
	{
		input_buf << "failed";
	}

	// send
	terminal->sendBlockdata(sendPackage, ICS_PROTOCOL_VERSION_11, IcsMsg_auth_response, input_buf.size());
//*/
}

// 标准状态上报:ok
void IcsClient::handleStdStatusReport(IcsProtocol& proto) throw(std::logic_error)
{
	/*

	
	MessageBuffer mbuffer(msghead + 1, msghead->length - sizeof(IcsMsgHead)-sizeof(uint32_t));	// 消息体内容

	uint32_t status_type;	// 标准状态类别
	mbuffer >> status_type;


	// 通用衡器
	if (status_type == 1)
	{
		uint8_t device_ligtht;	// 设备指示灯
		string device_status;	// 设备状态
		uint8_t cheat_ligtht;	// 作弊指示灯
		string cheat_status;	// 作弊状态
		float zero_point;		// 秤体零点	

		mbuffer >> device_ligtht;
		mbuffer.getLongStr(device_status);
		mbuffer >> cheat_ligtht >> cheat_status >> zero_point;

		if (!mbuffer.empty())
		{
			LOG4CPLUS_WARN(IcsTerminal::s_logger, "report standard status message has superfluous data:" << mbuffer.leftSize());
			return false;
		}

		CDatetime recv_time;
		getCurrentDatetime(&recv_time);

		// store into database
		std::vector<Variant> params;
		params.push_back(terminal->m_monitorID);	// 监测点编号
		params.push_back(int(device_ligtht));		// 设备指示灯
		params.push_back(device_status);	// 设备状态
		params.push_back(int(cheat_ligtht));	// 作弊指示灯
		params.push_back(cheat_status);		// 作弊状态
		params.push_back(zero_point);		//秤体零点
		params.push_back(recv_time);		//上报时间

		if (!IcsTerminal::s_core_api->CORE_CALL_SP(params, "sp_status_standard(:F1<char[32],in>,:F2<int,in>,:F3<char[1024],in>,:F4<int,in>,:F5<char[256],in>,:F6<float,in>,:F7<timestamp,in>)",
			terminal->m_dbName, terminal->m_dbSource))
		{
			LOG4CPLUS_ERROR(IcsTerminal::s_logger, "execute sp sp_status_standard error");
			return false;
		}
	}
	// 其它
	else
	{
		LOG4CPLUS_WARN(IcsTerminal::s_logger, "unknown standard status type:" << status_type);
	}
//*/
}

// 自定义状态上报
void IcsClient::handleDefStatusReport(IcsProtocol& proto) throw(std::logic_error)
{
	/*

	
	MessageBuffer mbuffer(msghead + 1, msghead->length - sizeof(IcsMsgHead)-sizeof(uint32_t));	// 消息体内容

	CDatetime status_time;		//	状态采集时间
	uint16_t status_count;		//	状态项个数


	uint16_t status_id = 0;		//	状态编号
	uint8_t status_light = 0;	//	状态指示灯
	uint8_t status_type = 0;	//	状态值类型
	string status_value;		//	状态值

	mbuffer >> status_time >> status_count;

	for (uint16_t i = 0; i<status_count; i++)
	{
		mbuffer >> status_id >> status_light >> status_type >> status_value;

		// 存入数据库
		LOG4CPLUS_DEBUG(IcsTerminal::s_logger, "light:" << (int)status_light << ",id:" << status_id << ",type:" << (int)status_type << ",value:" << status_value);

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
			LOG4CPLUS_ERROR(IcsTerminal::s_logger, "execute sp sp_status_userDef error");
			return false;
		}
	}

	if (!mbuffer.empty())
	{
		LOG4CPLUS_WARN(IcsTerminal::s_logger, "report defination status message has superfluous data:" << mbuffer.leftSize());
		return false;
	}
//*/
}

// 事件上报:ok
void IcsClient::handleEventsReport(IcsProtocol& proto) throw(std::logic_error)
{
	/*

	
	MessageBuffer mbuffer(msghead + 1, msghead->length - sizeof(IcsMsgHead)-sizeof(uint32_t));	// 消息体内容

	CDatetime event_time;	//	事件发生时间
	uint16_t event_count;	// 事件项个数

	uint16_t event_id = 0;	//	事件编号
	uint8_t event_type = 0;	//	事件值类型
	string event_value;		//	事件值

	CDatetime recv_time;	// 接收时间
	getCurrentDatetime(&recv_time);


	mbuffer >> event_time >> event_count;


	// 遍历取出全部事件
	for (uint16_t i = 0; i<event_count; i++)
	{
		mbuffer >> event_id >> event_type >> event_value;

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
			LOG4CPLUS_ERROR(IcsTerminal::s_logger, "execute sp sp_event_report error");
			return false;
		}

		// 发送给推送服务器
		push_buffer << terminal->m_monitorID << terminal->m_deviceKind << event_time << event_id << event_value;		// 监测点ID 发生时间 事件编号 事件值

		MsgPush::getInstance()->push(push_package, ICS_PROTOCOL_VERSION_11, PushMessage_Request, push_buffer.size());

	}

	if (!mbuffer.empty())
	{
		LOG4CPLUS_WARN(IcsTerminal::s_logger, "report event message has superfluous data:" << mbuffer.leftSize());
		return false;
	}
//*/
}

// 业务上报:ok
void IcsClient::handleBusinessReport(IcsProtocol& proto) throw(std::logic_error)
{
	/*


	MessageBuffer mbuffer(msghead + 1, msghead->length - sizeof(IcsMsgHead)-sizeof(uint32_t));	// 消息体内容

	CDatetime report_time;			// 业务采集时间 
	uint32_t business_no;	// 业务流水号
	uint32_t business_type;	// 业务类型

	CDatetime recv_time;	// 接收时间

	getCurrentDatetime(&recv_time);

	mbuffer >> report_time >> business_no >> business_type;

	if (terminal->m_lastBusSerialNum == business_no)
	{
		return true;	// 重复的业务流水号，直接忽略
	}
	terminal->m_lastBusSerialNum = business_no;	// 更新最近的业务流水号

	if (business_type == 1)	// 静态汽车衡
	{
		string cargo_num;	// 货物单号	
		string vehicle_num;	// 车号
		string consigness;	// 收货单位
		string cargo_name;	// 货物名称

		float weight1, weight2, weight3, weight4, unit_price, money;	// 毛重 皮重 扣重 净重 单价 金额
		uint8_t in_out;	// 进出

		mbuffer >> cargo_num >> vehicle_num >> consigness >> cargo_name >> weight1 >> weight2 >> weight3 >> weight4 >> unit_price >> money >> in_out;

		if (!mbuffer.empty())
		{
			LOG4CPLUS_WARN(IcsTerminal::s_logger, "report business message has superfluous data:" << mbuffer.leftSize());
			return false;
		}

		// store into database
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
			LOG4CPLUS_ERROR(IcsTerminal::s_logger, "execute sp sp_business_vehicle error");
			return false;
		}
	}
	else if (business_type == 2)	// 包装秤
	{
		uint8_t count;	// 秤数量
		mbuffer >> count;

		for (uint8_t i = 0; i < count; i++)	// 依次取出秤的称重数据
		{
			uint8_t number;		// 秤编号
			uint16_t amount;	// 称重次数
			float total_weight;		// 称重总重
			float single_weighet;// 单次重量

			mbuffer >> number >> amount >> total_weight >> single_weighet;

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
				LOG4CPLUS_ERROR(IcsTerminal::s_logger, "execute sp sp_business_pack error");
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

		mbuffer >> total_weight >> speed >> axle_num;

		// 处理各个轴重
		for (uint8_t i = 0; i < axle_num; i++)
		{
			uint16_t axle_weight;
			mbuffer >> axle_weight;
			snprintf(buff, sizeof(buff), "%u,", axle_weight);
			axle_str += buff;
		}

		if (!axle_str.empty())
		{
			axle_str.erase(axle_str.end() - 1);
		}

		mbuffer >> type_num;

		// 处理各个轴类型
		for (uint8_t i = 0; i < type_num; i++)
		{
			uint8_t axle_type;
			mbuffer >> axle_type;
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
			LOG4CPLUS_ERROR(IcsTerminal::s_logger, "execute sp sp_business_expressway error");
			return false;
		}

	}
	else
	{
		LOG4CPLUS_WARN(IcsTerminal::s_logger, "unknown business type:" << business_type);
	}

	if (!mbuffer.empty())
	{
		LOG4CPLUS_WARN(IcsTerminal::s_logger, "report business message has superfluous data:" << mbuffer.leftSize());
		return false;
	}
	//*/
}

// 终端回应参数查询
void IcsClient::handleParamQueryResponse(IcsProtocol& proto) throw(std::logic_error)
{
	/*

	
	MessageBuffer mbuffer(msghead + 1, msghead->length - sizeof(IcsMsgHead)-sizeof(uint32_t));	// 消息体内容

	uint32_t request_id;	// 请求id
	uint16_t param_count;	// 参数数量

	uint8_t net_id = 0;		//	子网编号
	uint16_t param_id = 0;	//	参数编号
	uint8_t param_type = 0;	//	参数值类型
	string param_value;		//	参数值

	mbuffer >> request_id >> param_count;

	for (uint16_t i = 0; i<param_count; i++)
	{
		mbuffer >> net_id >> param_id >> param_type >> param_value;
		// 存入数据库

	}

	if (!mbuffer.empty())
	{
		LOG4CPLUS_WARN(IcsTerminal::s_logger, "reply query paramter message has superfluous data:" << mbuffer.leftSize());
		return false;
	}

//*/
}

// 终端主动上报参数修改
void IcsClient::handleParamAlertReport(IcsProtocol& proto) throw(std::logic_error)
{
	/*

	
	MessageBuffer mbuffer(msghead + 1, msghead->length - sizeof(IcsMsgHead)-sizeof(uint32_t));	// 消息体内容

	CDatetime alert_time;	//	修改时间
	uint16_t param_count;	// 参数数量

	uint8_t net_id = 0;		//	子网编号
	uint16_t param_id = 0;	//	参数编号
	uint8_t param_type = 0;	//	参数值类型
	string param_value;		//	参数值

	mbuffer >> alert_time >> param_count;

	for (uint16_t i = 0; i<param_count; i++)
	{
		mbuffer >> net_id >> param_id >> param_type >> param_value;

		// 存入数据库

	}

	if (!mbuffer.empty())
	{
		LOG4CPLUS_WARN(IcsTerminal::s_logger, "report alert paramter message has superfluous data:" << mbuffer.leftSize());
		return false;
	}
//*/
}

// 终端回应参数修改
void IcsClient::handleParamModifyResponse(IcsProtocol& proto) throw(std::logic_error)
{
	/*

	
	MessageBuffer mbuffer(msghead + 1, msghead->length - sizeof(IcsMsgHead)-sizeof(uint32_t));	// 消息体内容

	uint32_t request_id;	// 请求id
	uint16_t param_count;	// 参数数量

	uint8_t net_id = 0;		//	子网编号
	uint16_t param_id = 0;	//	参数编号
	string alert_value;		//	修改结果

	mbuffer >> request_id >> param_count;

	for (uint16_t i = 0; i<param_count; i++)
	{
		mbuffer >> net_id >> param_id >> alert_value;
		// 存入数据库

	}

	if (!mbuffer.empty())
	{
		LOG4CPLUS_WARN(IcsTerminal::s_logger, "reply modify paramter message has superfluous data:" << mbuffer.leftSize());
		return false;
	}
//*/
}

// 终端发送时钟同步请求
void IcsClient::handleDatetimeSync(IcsProtocol& proto) throw(std::logic_error)
{
	/*

	
	MessageBuffer mbuffer(msghead + 1, msghead->length - sizeof(IcsMsgHead)-sizeof(uint32_t));	// 消息体内容

	CDatetime dt1, dt2;
	mbuffer >> dt1;
	if (!mbuffer.empty())
	{
		LOG4CPLUS_WARN(IcsTerminal::s_logger, "request datetime sync message has superfluous data:" << mbuffer.leftSize());
		return false;
	}

	// reply message
	boost::shared_ptr<CBlock> sendPackage = IcsTerminal::s_core_api->CORE_CREATE_BLOCK();
	MessageBuffer input_buf(sendPackage->getpointer<char>(sizeof(IcsMsgHead), 64), 64);

	getCurrentDatetime(&dt2);
	input_buf << dt1 << dt2 << dt2;

	// send
	terminal->sendBlockdata(sendPackage, ICS_PROTOCOL_VERSION_11, IcsMsg_datetime_sync_response, input_buf.size());
//*/
}

// 终端上报日志
void IcsClient::handleLogReport(IcsProtocol& proto) throw(std::logic_error)
{
	/*

	
	MessageBuffer mbuffer(msghead + 1, msghead->length - sizeof(IcsMsgHead)-sizeof(uint32_t));	// 消息体内容

	CDatetime status_time;		//	时间
	uint8_t log_level;			//	日志级别
	uint8_t encode_type = 0;	//	编码方式
	string log_value;			//	日志内容

	mbuffer >> status_time >> log_level >> encode_type >> log_value;

	if (encode_type == 1)	// 0-UTF-8,1-GB2312
	{
		if (!IcsTerminal::charsetConv("GB2312", "UTF-8", log_value))
		{
			LOG4CPLUS_ERROR(IcsTerminal::s_logger, "编码转换失败");
			return false;
		}
	}
	else if (encode_type != 0)
	{
		LOG4CPLUS_ERROR(IcsTerminal::s_logger, "unkonw encode type: " << (int)encode_type);
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
		LOG4CPLUS_ERROR(IcsTerminal::s_logger, "execute sp sp_log_report error");
		return false;
	}


	if (!mbuffer.empty())
	{
		LOG4CPLUS_WARN(IcsTerminal::s_logger, "report log message has superfluous data:" << mbuffer.leftSize());
		return false;
	}
//*/
}

// 终端发送心跳到中心
void IcsClient::handleHeartbeat(IcsProtocol& proto) throw(std::logic_error)
{
}

// 终端拒绝升级请求
void IcsClient::handleDenyUpgrade(IcsProtocol& proto) throw(std::logic_error)
{
	/*

	MessageBuffer mbuffer(msghead + 1, msghead->length - sizeof(IcsMsgHead)-sizeof(uint32_t));	// 消息体内容

	uint32_t request_id;	// 请求id
	string reason;	// 拒绝升级原因

	mbuffer >> request_id >> reason;

	// verify length
	if (!mbuffer.empty())
	{
		LOG4CPLUS_WARN(IcsTerminal::s_logger, "reply deny upgrade message has superfluous data:" << mbuffer.leftSize());
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
		LOG4CPLUS_ERROR(IcsTerminal::s_logger, "execute sp sp_upgrade_refuse error");
		return false;
	}

	//*/

}

// 终端接收升级请求
void IcsClient::handleAgreeUpgrade(IcsProtocol& proto) throw(std::logic_error)
{
	/*

	
	MessageBuffer mbuffer(msghead + 1, msghead->length - sizeof(IcsMsgHead)-sizeof(uint32_t));	// 消息体内容

	uint32_t request_id;	// 文件id

	mbuffer >> request_id;

	if (!mbuffer.empty())
	{
		LOG4CPLUS_ERROR(IcsTerminal::s_logger, "reply agree upgrade message has superfluous data:" << mbuffer.leftSize());
		return false;
	}

	vector<Variant> params;
	params.push_back(terminal->m_monitorID);
	params.push_back(int(request_id));

	if (!IcsTerminal::s_core_api->CORE_CALL_SP(params,
		"sp_upgrade_accept(:F1<char[33],in>,:F2<int,in>)",
		terminal->m_dbName, terminal->m_dbSource))
	{
		LOG4CPLUS_ERROR(IcsTerminal::s_logger, "execute sp sp_upgrade_accept error");
		return false;
	}
//*/
}

// 索要升级文件片段
void IcsClient::handleRequestFile(IcsProtocol& proto) throw(std::logic_error)
{
	/*

	
	MessageBuffer mbuffer(msghead + 1, msghead->length - sizeof(IcsMsgHead)-sizeof(uint32_t));	// 消息体内容

	uint32_t file_id, request_id, fragment_offset, received_size;

	uint16_t fragment_length;

	mbuffer >> file_id >> request_id >> fragment_offset >> fragment_length >> received_size;

	if (!mbuffer.empty())
	{
		LOG4CPLUS_WARN(IcsTerminal::s_logger, "request upgrade file message has superfluous data:" << mbuffer.leftSize());
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
		LOG4CPLUS_ERROR(IcsTerminal::s_logger, "execute sp sp_upgrade_set_progress error");
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
			LOG4CPLUS_WARN(IcsTerminal::s_logger, "require file offset (" << fragment_offset << ") is more than file's length (" << fileInfo->file_length << ")");
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
void IcsClient::handleUpgradeResult(IcsProtocol& proto) throw(std::logic_error)
{
	/*

	
	MessageBuffer mbuffer(msghead + 1, msghead->length - sizeof(IcsMsgHead)-sizeof(uint32_t));	// 消息体内容

	uint32_t request_id;	// 文件id
	string upgrade_result;	// 升级结果

	mbuffer >> request_id >> upgrade_result;

	if (!mbuffer.empty())
	{
		LOG4CPLUS_WARN(IcsTerminal::s_logger, "report upgrade result message has superfluous data:" << mbuffer.leftSize());
		return false;
	}

	vector<Variant> params;
	params.push_back(int(request_id));
	params.push_back(upgrade_result);

	if (!IcsTerminal::s_core_api->CORE_CALL_SP(params,
		"sp_upgrade_result(:F1<int,in>,:F3<char[126],in>)",
		terminal->m_dbName, terminal->m_dbSource))
	{
		LOG4CPLUS_ERROR(IcsTerminal::s_logger, "execute sp sp_upgrade_result error");
		return false;
	}
//*/

}

// 终端确认取消升级
void IcsClient::handleUpgradeCancelAck(IcsProtocol& proto) throw(std::logic_error)
{
	/*

	
	MessageBuffer mbuffer(msghead + 1, msghead->length - sizeof(IcsMsgHead)-sizeof(uint32_t));	// 消息体内容

	uint32_t request_id;	// 文件id

	mbuffer >> request_id;
	if (!mbuffer.empty())
	{
		LOG4CPLUS_WARN(IcsTerminal::s_logger, "report upgrade cancel ack message has superfluous data:" << mbuffer.leftSize());
		return false;
	}

	vector<Variant> params;
	params.push_back(int(request_id));

	if (!IcsTerminal::s_core_api->CORE_CALL_SP(params,
		"sp_upgrade_cancel_ack(:F1<int,in>)",
		terminal->m_dbName, terminal->m_dbSource))
	{
		LOG4CPLUS_ERROR(IcsTerminal::s_logger, "execute sp sp_upgrade_cancel_ack error");
		return false;
	}
//*/
}

//----------------------------------------------------------------------------------//



}// end namespace ics
