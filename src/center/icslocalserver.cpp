﻿

#include "icslocalserver.hpp"
#include "mempool.hpp"
#include "log.hpp"
#include "database.hpp"
#include "downloadfile.hpp"
#include "util.hpp"
#include "icsprotocol.hpp"
#include "database.hpp"
#include "downloadfile.hpp"



extern ics::DataBase g_database;
extern ics::IcsConfig g_configFile;


namespace ics {

//---------------------------ics terminal---------------------------//
IcsTerminalClient::IcsTerminalClient(IcsLocalServer& localServer, socket&& s, const char* name)
	:  _baseType(std::move(s), name)
	, m_localServer(localServer)
{
}

IcsTerminalClient::~IcsTerminalClient()
{
	if (!_baseType::m_replaced && !m_monitorID.empty())
	{
		OtlConnectionGuard connGuard(g_database);

		otl_stream s(1
			, "{ call sp_offline(:id<char[33],in>,:ip<char[17],in>,:port<int,in>) }"
			, connGuard.connection());

		s << m_monitorID << m_localServer.getWebIp() << m_localServer.getWebPort();
	}
}

// 处理底层消息
void IcsTerminalClient::handle(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	auto id = request.getHead()->getMsgID();
	if (id != T2C_auth_request_0x0101 && m_gwid.empty())
	{
		throw IcsException("must authrize at first step");
	}
	switch (id)
	{
	case T2C_auth_request_0x0101:
		handleAuthRequest(request, response);
		break;

	case T2C_heartbeat_0x0b01:
		handleHeartbeat(request, response);
		break;

	case T2C_std_status_report_0x0301:
		handleStdStatusReport(request, response);
		break;

	case T2C_def_status_report_0x0401:
		handleDefStatusReport(request, response);
		break;

	case T2C_event_report_0x0501:
		handleEventsReport(request, response);
		break;

	case T2C_bus_report_0x0901:
		handleBusinessReport(request, response);
		break;

	case T2C_gps_report_0x0902:
		handleGpsReport(request, response);
		break;

	case T2C_datetime_sync_request_0x0a01:
		handleDatetimeSync(request, response);
		break;

	case T2C_log_report_0x0c01:
		handleLogReport(request, response);
		break;

	case T2C_param_alter_report_0x0701:
		handleParamAlertReport(request, response);
		break;

	case T2C_param_modiy_response_0x0802:
		handleParamModifyResponse(request, response);
		break;

	case T2C_param_query_response_0x0602:
		handleParamQueryResponse(request, response);
		break;

	case T2C_control_response_0x0d02:
		handleControlAck(request, response);
		break;

	case T2C_upgrade_agree_0x0203:
		handleAgreeUpgrade(request, response);
		break;

	case T2C_upgrade_deny_0x0202:
		handleDenyUpgrade(request, response);
		break;

	case T2C_upgrade_file_request_0x0204:
		handleRequestFile(request, response);
		break;

	case T2C_upgrade_result_report_0x0207:
		handleUpgradeResult(request, response);
		break;

	case T2C_upgrade_cancel_ack_0x0209:
		handleUpgradeCancelAck(request, response);
		break;

	default:
		throw IcsException("unknown terminal message id = 0x%04x ", (uint16_t)id);
		break;
	}
}

// 处理平层消息
void IcsTerminalClient::dispatch(ProtocolStream& request) throw(IcsException, otl_exception)
{
	// 消息结构：网关ID 消息ID 消息体内容(请求ID 文件ID)
	ShortString terminalName;
	uint16_t messageID;
	uint32_t requestID;
	request >> terminalName >> messageID;

	// 发送到该链接对端
	ProtocolStream response(ProtocolStream::OptType::writeType, g_memoryPool.get());
	response.initHead((MessageId)messageID, false);
	response << request;

	_baseType::trySend(response);
}

// 出错处理
void IcsTerminalClient::error() throw()
{
	if (!_baseType::m_replaced && !m_gwid.empty())
	{
		m_localServer.removeTerminalClient(m_gwid);
		try {
			OtlConnectionGuard connGuard(g_database);
			otl_stream s(1
				, "{ call sp_offline(:gwid<char[33],in>,:ip<char[17],in>,:port<int,in>) }"
					, connGuard.connection());
			s << m_gwid << m_localServer.getWebIp() << m_localServer.getWebPort();
		}
		catch (otl_exception& ex)
		{
			LOG_WARN("otl_exception:" << ex.msg);
		}
		m_gwid.clear();
	}
}

// 终端认证
void IcsTerminalClient::handleAuthRequest(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	if (!m_gwid.empty())
	{
		LOG_DEBUG(m_gwid << " ignore repeat authrize message");
		response.initHead(MessageId::C2T_auth_response_0x0102, request.getHead()->getSendNum());
		response << ShortString("ok") << m_localServer.getHeartbeatTime();
		return;
	}

	// auth info
	string gwPwd, extendInfo;

	request >> m_gwid >> gwPwd >> m_deviceKind >> extendInfo;

	request.assertEmpty();

	OtlConnectionGuard connGuard(g_database);

	otl_stream authroizeStream(1,
		"{ call sp_authroize(:gwid<char[33],in>,:pwd<char[33],in>,@ret,@monitorID,@monitorName) }",
		connGuard.connection());

	authroizeStream << m_gwid << gwPwd;
	authroizeStream.close();

	otl_stream getStream(1, "select @ret :#ret<int>,@monitorID :#monitorID<char[32]>,@monitorName :#monitorName<char[32]>", connGuard.connection());

	int ret = 2;
	string monitorID, monitorName;

	getStream >> ret >> monitorID >> monitorName;

	response.initHead(MessageId::C2T_auth_response_0x0102, false);

	if (ret == 0)	// 成功
	{
		m_monitorID = std::move(monitorID); // 保存检测点id

		otl_stream onlineStream(1
			, "{ call sp_online(:gwid<char[33],in>,:monitorID<char[33],in>,:devKind<int,in>,:ip<char[16],in>,:port<int,in>) }"
			, connGuard.connection());

		onlineStream << m_gwid << m_monitorID << (int)m_deviceKind << m_localServer.getWebIp() << m_localServer.getWebPort();

		response << ShortString("ok") << m_localServer.getHeartbeatTime();

		m_localServer.addTerminalClient(m_gwid, shared_from_this());

		LOG_INFO("gwid [" << m_gwid << "] created on " << this->name());

		this->setName(m_gwid + "@" + this->name());
	}
	else
	{
		m_gwid.clear();
		response << ShortString("failed");
	}

}

// 标准状态上报
void IcsTerminalClient::handleStdStatusReport(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception, otl_exception)
{
	uint32_t status_type;	// 标准状态类别

	request >> status_type;

	// 通用衡器
	if (status_type == 1)
	{
		uint8_t device_ligtht;	// 设备指示灯
		LongString device_status;	// 设备状态
		uint8_t cheat_ligtht;	// 作弊指示灯
		string cheat_status;	// 作弊状态
		float zero_point;		// 秤体零点	

		request >> device_ligtht >> device_status >> cheat_ligtht >> cheat_status >> zero_point;

		request.assertEmpty();

		IcsDataTime recv_time;
		ics::getIcsNowTime(recv_time);

		OtlConnectionGuard connGuard(g_database);

		otl_stream o(1,
			"{ call sp_status_standard(:monitorID<char[16],in>,:gwID<char[16],in>,:devFlag<int,in>,:devStat<char[512],in>,:cheatFlag<int,in>,:cheatStat<char[512],in>,:zeroPoint<float,in>,:recvTime<timestamp,in>) }"
			, connGuard.connection());

		o << m_monitorID << m_gwid << (int)device_ligtht << device_status << (int)cheat_ligtht << cheat_status << zero_point << recv_time;
	}
	// 其它
	else
	{
		LOG_WARN(m_monitorID << "recv unknown standard status type:" << status_type);
	}
}

// 自定义状态上报
void IcsTerminalClient::handleDefStatusReport(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
// 	CDatetime status_time;		//	状态采集时间
// 	uint16_t status_count;		//	状态项个数
// 	uint16_t status_id = 0;		//	状态编号
// 	uint8_t status_light = 0;	//	状态指示灯
// 	uint8_t status_type = 0;	//	状态值类型
// 	string status_value;		//	状态值

//	request >> status_time >> status_count;

	// 		for (uint16_t i = 0; i<status_count; i++)
	// 		{
	// 		request >> status_id >> status_light >> status_type >> status_value;
	// 
	// 		// 存入数据库
	// 		LOG4CPLUS_DEBUG("light:" << (int)status_light << ",id:" << status_id << ",type:" << (int)status_type << ",value:" << status_value);

	// 		// store into database
	// 		std::vector<Variant> params;
	// 		params.push_back(terminal->m_monitorID);// 监测点编号
	// 		params.push_back(int(status_id));		// 状态编号
	// 		params.push_back(int(status_light));	// 状态指示灯
	// 		params.push_back(int(status_type));		// 状态值类型
	// 		params.push_back(status_value);			//状态值
	// 		params.push_back(status_time);			//状态采集时间

	//		if (!IcsTerminalClient::s_core_api->CORE_CALL_SP(params, "sp_status_userDef(:F1<char[32],in>,:F2<int,in>,:F3<int,in>,:F4<int,in>,:F5<char[256],in>,:F6<timestamp,in>)",
	//		terminal->m_dbName, terminal->m_dbSource))

}

// 事件上报
void IcsTerminalClient::handleEventsReport(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	IcsDataTime event_time, recv_time;	//	事件发生时间 接收时间
	uint16_t event_count;	// 事件项个数

	uint16_t event_id = 0;	//	事件编号
	uint8_t event_type = 0;	//	事件值类型
	string event_value;		//	事件值

	getIcsNowTime(recv_time);

	request >> event_time >> event_count;


	OtlConnectionGuard connGuard(g_database);

	otl_stream eventStream(1
		, "{ call sp_event_report(:id<char[33],in>,:devKind<int,in>,:eventID<int,in>,:eventType<int,in>,:eventValue<char[256],in>,:eventTime<timestamp,in>,:recvTime<timestamp,in>) }"
		, connGuard.connection());


	// 遍历取出全部事件
	for (uint16_t i = 0; i < event_count; i++)
	{
		request >> event_id >> event_type >> event_value;

		eventStream << m_monitorID << (int)m_deviceKind << (int)event_id << (int)event_type << event_value << event_time << recv_time;

		// 发送给推送服务器: 监测点ID 发生时间 事件编号 事件值
		ProtocolStream pushStream(ProtocolStream::OptType::writeType, g_memoryPool.get());
		pushStream << m_monitorID << m_deviceKind << event_time << event_id << event_value;

		m_localServer.getPushSystem().send(pushStream);
	}
	request.assertEmpty();
}

// 业务上报
void IcsTerminalClient::handleBusinessReport(ProtocolStream& request, ProtocolStream& response)  throw(IcsException, otl_exception)
{
	IcsDataTime report_time, recv_time;			// 业务采集时间 接收时间
	uint32_t business_no;	// 业务流水号
	uint32_t business_type;	// 业务类型

	getIcsNowTime(recv_time);

	request >> report_time >> business_no >> business_type;

	if (m_lastBusSerialNum == business_no)	// 重复的业务流水号，直接忽略
	{
		response.initHead(MessageId::MessageId_min_0x0000, false);
		return;
	}

	OtlConnectionGuard connGuard(g_database);

	m_lastBusSerialNum = business_no;	// 更新最近的业务流水号

	if (business_type == 1)	// 静态汽车衡
	{
		ShortString cargo_num;	// 货物单号	
		ShortString vehicle_num;	// 车号
		ShortString consigness;	// 收货单位
		ShortString cargo_name;	// 货物名称

		float weight1, weight2, weight3, weight4, unit_price, money;	// 毛重 皮重 扣重 净重 单价 金额
		uint8_t in_out;	// 进出

		request >> cargo_num >> vehicle_num >> consigness >> cargo_name >> weight1 >> weight2 >> weight3 >> weight4 >> unit_price >> money >> in_out;

		otl_stream s(1
			, "{ call `ics_vehicle`.sp_business_vehicle(:id<char[33],in>,:num<int,in>,:cargoNum<char[126],in>,:vehNum<char[126],in>"
			",:consigness<char[126],in>,:cargoName<char[126],in>,:weight1<float,in>,:weight2<float,in>,:weight3<float,in>,:weight4<float,in>"
			",:unitPrice<float,in>,:money<float,in>,:inOrOut<int,in>,:reportTime<timestamp,in>,:recvTime<timestamp,in>) }"
			, connGuard.connection());

		s << m_monitorID << (int)business_no << cargo_num << vehicle_num
			<< consigness << cargo_name << weight1 << weight2 << weight3 << weight4
			<< unit_price << money << (int)in_out << report_time << recv_time;

	}
	else if (business_type == 2)	// 包装秤
	{
		uint8_t count;	// 秤数量
		request >> count;

		otl_stream s(1
			, "{ call `ics_packing`.sp_business_pack(:id<char[33],in>,:num<int,in>,:amount<int,in>,:weight<int,in>,:sWeight<int,in>,:F6<float,in>,:reportTime<timestamp,in>,:recvTime<timestamp,in>) }"
			, connGuard.connection());

		for (uint8_t i = 0; i < count; i++)	// 依次取出秤的称重数据
		{
			uint8_t number;		// 秤编号
			uint16_t amount;	// 称重次数
			float total_weight;		// 称重总重
			float single_weighet;// 单次重量

			request >> number >> amount >> total_weight >> single_weighet;

			s << m_monitorID << business_no << (int)amount << total_weight << single_weighet << report_time << recv_time;
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

		otl_stream s(1
			, "{ call `ics_highway`.sp_business_expressway(:id<char[33],in>,:num<int,in>,:weight<int,in>,:speed<double,in>,:axleNum<int,in>,:axleStr<char[256],in>,:typeNum<int,in>,:typeStr<char[256],in>,:reportTime<timestamp,in>,:recvTime<timestamp,in>) }"
			, connGuard.connection());

		s << m_monitorID << (int)business_no << (int)total_weight << ((float)speed)*0.1 << (int)axle_num << axle_str << (int)type_num << type_str << report_time << recv_time;
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

		ShortString tubID;
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
			, connGuard.connection());

		s << m_monitorID << (int)business_no << report_time << recv_time
			<< (int)weightFlag.mode << (int)weightFlag.unit << (int)weightFlag.card << (int)weightFlag.flow << (int)weightFlag.evalution
			<< tubID << (int)tubVolumn << (int)weight << (int)driverID
			<< (int)postionFlag.longitude_flag << (int)longitude << (int)postionFlag.latitude_flag << (int)latitude << (int)postionFlag.signal << (int)height << (int)speed;
	}
	else if (business_type == 5)	// 高速治超
	{
		ShortString vehicleID;	// 车牌号
		IcsDataTime checkTime1, checkTime2; // 预检时间,复检时间
		uint8_t axleCount1, axleCount2;	// 预检轴数,复检轴数
		uint32_t totalWeight1, totalWeight2, limitWeight1, limitWeight2, overWeight;

		request >> vehicleID >> checkTime2 >> totalWeight2 >> limitWeight2 >> axleCount2 >> checkTime1 >> totalWeight1 >> limitWeight1 >> overWeight >> axleCount1;
		request.assertEmpty();

		otl_stream s(1
			, "{ call `ics_freewayOverloadControl`.sp_business_overload(:id<char[33],in>,:busNum<int,in>,:recvTime<timestamp,in>,:vehNum<char[256],in>"
			",:checkT1<timestamp,in>,:totalW1<int,in>,:limitW1<int,in>,:axleCount1<int,in>,:overW<int,in>"
			",:checkT2<timestamp,in>,:totalW2<int,in>,:limitW2<int,in>,:axleCount2<int,in>) }"
			, connGuard.connection());

		s << m_monitorID << (int)business_no << recv_time << vehicleID 
			<< checkTime1 << (int)totalWeight1 << (int)limitWeight1 << (int)axleCount1 << (int)overWeight
			<< checkTime2 << (int)totalWeight2 << (int)limitWeight2 << (int)axleCount2;
	}
	else if (business_type == 6)	// 高速治超汇报
	{
		uint32_t vehicleCount;

		request >> vehicleCount;
		request.assertEmpty();

		otl_stream s(1
			, "{ call `ics_freewayOverloadControl`.sp_business_dayreport(:id<char[33],in>,:recvTime<timestamp,in>,:reportTime<timestamp,in>,:count<int,in>) }"
			, connGuard.connection());

		s << m_monitorID << recv_time << report_time << (int)vehicleCount;
	}
	else
	{
		throw IcsException("unknown business type=%d", business_type);
	}

	request.assertEmpty();

}

// GPS上报
void IcsTerminalClient::handleGpsReport(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
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

	OtlConnectionGuard connGuard(g_database);

	otl_stream s(1
		, "{ call `ics_canchu`.sp_gps_report(:id<char[33],in>,:logFlag<int,in>,:logitude<int,in>,:laFlag<int,in>,:latitude<int,in>,:signal<int,in>,:height<int,in>,:speed<int,in>) }"
		, connGuard.connection());

	s << m_monitorID << (int)postionFlag.longitude_flag << (int)longitude << (int)postionFlag.latitude_flag << (int)latitude << (int)postionFlag.signal << (int)height << (int)speed;
}

// 终端回应参数查询
void IcsTerminalClient::handleParamQueryResponse(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	uint32_t request_id;	// 请求id
	uint16_t param_count;	// 参数数量

	uint16_t net_id = 0;		//	子网编号
	uint16_t param_id = 0;	//	参数编号
	uint8_t param_type = 0;	//	参数值类型
	string param_value;		//	参数值

	request >> request_id >> param_count;

	OtlConnectionGuard connGuard(g_database);
	otl_stream s(1
		, "{ call `ics_base`.sp_param_query_result(:requestID<int,in>,:netID<int,in>,:paramID<int,in>,:paramValue<char[256],in>) }"
		, connGuard.connection());
	for (uint16_t i = 0; i<param_count; i++)
	{
		request >> net_id >> param_id >> param_type >> param_value;
		s << (int)request_id << (int)net_id << (int)param_id << param_value;	// 存入数据库
	}

}

// 终端主动上报参数修改
void IcsTerminalClient::handleParamAlertReport(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	IcsDataTime alert_time;	//	修改时间
	uint16_t param_count;	// 参数数量

	uint16_t net_id = 0;	//	子网编号
	uint16_t param_id = 0;	//	参数编号
	uint8_t param_type = 0;	//	参数值类型
	string param_value;		//	参数值

	request >> alert_time >> param_count;

	OtlConnectionGuard connGuard(g_database);
	otl_stream s(1
		, "{ call `ics_base`.sp_param_report_modify(:monitorID<char[32],in>,:devKind<int,in>,:modifyTime<timestamp,in>,:netID<int,in>,:paramID<int,in>,:result<char[256],in>) }"
		, connGuard.connection());
	for (uint16_t i = 0; i < param_count; i++)
	{
		request >> net_id >> param_id >> param_type >> param_value;
		s << m_monitorID << (int)m_deviceKind << alert_time << (int)net_id << (int)param_id << param_value;	// 存入数据库
	}
}

// 终端回应参数修改
void IcsTerminalClient::handleParamModifyResponse(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	uint32_t request_id;	// 请求id
	uint16_t param_count;	// 参数数量

	uint16_t net_id = 0;		//	子网编号
	uint16_t param_id = 0;	//	参数编号
	string result;		//	修改结果

	request >> request_id >> param_count;

	OtlConnectionGuard connGuard(g_database);
	otl_stream s(1
		, "{ call `ics_base`.sp_param_modify_result(:requestID<int,in>,:netID<int,in>,:paramID<int,in>,:result<char[256],in>) }"
		, connGuard.connection());
	for (uint16_t i = 0; i < param_count; i++)
	{
		request >> net_id >> param_id >> result;
		s << (int)request_id << (int)net_id << (int)param_id << result;	// 存入数据库
	}
}

// 终端发送时钟同步请求
void IcsTerminalClient::handleDatetimeSync(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	IcsDataTime dt1, dt2;
	request >> dt1;
	request.assertEmpty();

	getIcsNowTime(dt2);

	response.initHead(MessageId::C2T_datetime_sync_response_0x0a02, false);
	response << dt1 << dt2 << dt2;
}

// 终端上报日志
void IcsTerminalClient::handleLogReport(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	IcsDataTime status_time;		//	时间
	uint8_t log_level;			//	日志级别
	uint8_t encode_type = 0;	//	编码方式
	string log_value;			//	日志内容

	request >> status_time >> log_level >> encode_type >> log_value;

	request.assertEmpty();

	if (encode_type == 1)	// 0-UTF-8,1-GB2312
	{
		character_convert("GB2312", log_value, log_value.length(), "UTF-8", log_value);
	}
	else if (encode_type != 0)
	{
		LOG_ERROR("unkonwn encode type: " << (int)encode_type);
		throw IcsException("undefined encode type");
	}

	OtlConnectionGuard connGuard(g_database);
	otl_stream s(1
		, "{ call sp_log_report(:id<char[32],in>,:logtime<timestamp,in>,:logLevel<int,in>,:logValue<char[256],in>) }"
		, connGuard.connection());

	s << m_monitorID << status_time << (int)log_level << log_value;
}

// 终端发送心跳到中心
void IcsTerminalClient::handleHeartbeat(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{

}

// 终端拒绝升级请求
void IcsTerminalClient::handleDenyUpgrade(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	uint32_t request_id;	// 请求id
	string reason;	// 拒绝升级原因

	request >> request_id >> reason;

	request.assertEmpty();

	OtlConnectionGuard connGuard(g_database);
	otl_stream s(1
		, "{ call sp_upgrade_refuse(:id<char[33],in>,:reqID<int,in>,:reason<char[126],in>) }"
		, connGuard.connection());

	s << m_monitorID << int(request_id) << reason;
}

// 终端接收升级请求
void IcsTerminalClient::handleAgreeUpgrade(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	uint32_t request_id;	// 请求id
	request >> request_id;
	request.assertEmpty();

	OtlConnectionGuard connGuard(g_database);
	otl_stream s(1
		, "{ call sp_upgrade_accept(:id<char[33],in>,:reqID<int,in>) }"
		, connGuard.connection());

	s << m_monitorID << int(request_id);
}

// 索要升级文件片段
void IcsTerminalClient::handleRequestFile(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	uint32_t file_id, request_id, fragment_offset, received_size;
	uint16_t fragment_length;

	request >> file_id >> request_id >> fragment_offset >> fragment_length >> received_size;

	request.assertEmpty();


	// 设置升级进度(查询该请求id对应的状态)
	OtlConnectionGuard connGuard(g_database);
	otl_stream s(1
		, "{ call sp_upgrade_set_progress(:requestID<int,in>,:recvSize<int,in>,@stat) }"
		, connGuard.connection());

	s << (int)request_id << (int)received_size;

	otl_stream queryResutl(1, "select @stat :#<int>", connGuard.connection());

	int result = 99;

	queryResutl >> result;

	// 查找文件
	auto fileInfo = FileUpgradeManager::getInstance()->getFileInfo(file_id);


	// 查看是否已取消升级
	if (fragment_length > 0 && result == 0 && fileInfo)	// 正常状态且找到该文件
	{
		if (fragment_offset > fileInfo->file_length)	// 超出文件大小
		{
			throw IcsException("require file offset [%d] is more than file's length [%d]", fragment_offset, fileInfo->file_length);
		}
		else if (fragment_offset + fragment_length > fileInfo->file_length)	// 超过文件大小则取剩余大小
		{
			fragment_length = fileInfo->file_length - fragment_offset;
		}

		if (fragment_length > UPGRADE_FILE_SEGMENG_SIZE)	// 限制文件片段最大为缓冲区剩余长度
		{
			fragment_length = UPGRADE_FILE_SEGMENG_SIZE;
		}

		response.initHead(C2T_upgrade_file_response_0x0206, false);
		response << file_id << request_id << fragment_offset << fragment_length;
		response.append((char*)fileInfo->file_content + fragment_offset, fragment_length);
	}
	else	// 无升级事务
	{
		response.initHead(MessageId::C2T_upgrade_not_found_0x0205, request.getHead()->getAckNum());
	}
}

// 升级文件传输结果
void IcsTerminalClient::handleUpgradeResult(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	uint32_t request_id;	// 文件id
	string upgrade_result;	// 升级结果

	request >> request_id >> upgrade_result;

	request.assertEmpty();

	OtlConnectionGuard connGuard(g_database);
	otl_stream s(1
		, "{ call sp_upgrade_result(:F1<int,in>,:F3<char[126],in>) }"
		, connGuard.connection());

	s << (int)request_id << upgrade_result;
}

// 终端确认取消升级
void IcsTerminalClient::handleUpgradeCancelAck(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	uint32_t request_id;	// 请求id

	request >> request_id;

	request.assertEmpty();

	OtlConnectionGuard connGuard(g_database);
	otl_stream s(1
		, "{ call sp_upgrade_cancel_ack(:F1<int,in>) }"
		, connGuard.connection());

	s << (int)request_id;
}

// 终端回应控制结果
void IcsTerminalClient::handleControlAck(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	uint32_t request_id;	// 请求id
	uint16_t operator_id;	// 操作id
	ShortString result;		// 操作结果

	request >> request_id >> operator_id >> result;
	request.assertEmpty();

	OtlConnectionGuard connGuard(g_database);
	otl_stream s(1
		, "{ call sp_control_result(:requestID<int,in>,:operatorID<int,in>,:result<char[256],in>) }"
		, connGuard.connection());

	s << (int)request_id << (int)operator_id << result;;
}

//---------------------------ics web---------------------------//
IcsWebClient::IcsWebClient(IcsLocalServer& localServer, socket&& s)
: _baseType(std::move(s), "Web")
	, m_localServer(localServer)
{
}

// 处理底层消息
void IcsWebClient::handle(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	switch (request.getHead()->getMsgID())
	{
	case MessageId::W2C_send_to_ics_terminal_0x2001:
		handleICSForward(request, response);
		break;

	case MessageId::W2C_connect_remote_request_0x2002:
		handleConnectRemote(request, response);
		break;

	case MessageId::W2C_disconnect_remote_0x2003:
		handleDisconnectRemote(request, response);
		break;

	case MessageId::W2C_send_to_remote_terminal_0x2004:
		handleRemoteForward(request, response);
		break;

	default:
		throw IcsException("unknown web message id: %04x", request.getHead()->getMsgID());
		break;
	}
}

// 处理平层消息
void IcsWebClient::dispatch(ProtocolStream& request) throw(IcsException, otl_exception)
{
	throw IcsException("WebHandler never handle message, id: %d ", request.getHead()->getMsgID());
}

// 出错处理
void IcsWebClient::error() throw()
{

}

// 转发到ICS对应终端
void IcsWebClient::handleICSForward(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	// 消息结构：网关ID 消息ID 消息体内容(请求ID 文件ID)
	ShortString gwid;
	uint16_t messageID;
	uint32_t requestID;
	request >> gwid >> messageID >> requestID;
	request.rewind();

	if (!gwid.empty())
	{
		bool ret = false;
		// 根据终端ID转发该消息
		auto conn = m_localServer.findTerminalClient(gwid);

		if (conn)
		{
			try {
				conn->dispatch(request);
				ret = true;
			}
			catch (IcsException& ex)
			{
				LOG_ERROR("forward to terminal " << gwid << " error:" << ex.message());
			}
		}
		else
		{
			LOG_ERROR("forward terminal " << gwid << " not found");
		}

		// 转发结果记录到数据库
		{
			OtlConnectionGuard connection(g_database);
			otl_stream s(1, "{ call sp_web_command_status(:requestID<int,in>,:msgID<int,in>,:stat<int,in>) }", connection.connection());
			s << (int)requestID << (int)messageID << (ret ? 0 : 1);
		}
	}
}

// 连接远端子服务器
void IcsWebClient::handleConnectRemote(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	ShortString remoteID;
	request >> remoteID;
	request.assertEmpty();

	if (remoteID.empty())
	{
		LOG_WARN(this->name() << " enterprise is empty");
		return;
	}

	/// 查询是否已经连接
	if (m_localServer.findRemoteProxy(remoteID))
	{
		LOG_INFO("The remote enterprise " << remoteID << " is conneted");
		return;
	}

	// 从数据库中查询该远端地址
	OtlConnectionGuard connGuard(g_database);
	otl_stream queryStream(1
		, "SELECT t.IP,t.PORT FROM b_enterprise_t t	WHERE t.ENTERPRISE_CODE=:id<char[33]> AND t.HAVING_SUBCOMM=1"
		, connGuard.connection());

	queryStream << remoteID;

	string remoteIP;
	int remotePort;

	if (!queryStream.eof())
	{
		queryStream >> remoteIP >> remotePort;
	}

	// 回应web连接结果:0-成功,1-失败

	if (!remoteIP.empty() || remotePort != 0)
	{
		// 连接该远端
		asio::ip::tcp::socket remoteSocket(m_localServer.getIoService());
		asio::ip::tcp::endpoint endpoint(asio::ip::address::from_string(remoteIP), remotePort);
		asio::error_code ec;
		remoteSocket.connect(endpoint, ec);

		if (!ec)
		{
			auto conn = std::make_shared<IcsRemoteProxyClient>(m_localServer, std::move(remoteSocket), remoteID);
			conn->start();
			conn->requestAuthrize();
		}
		else
		{
			LOG_ERROR(this->name() << " connect to " << remoteIP << ":" << remotePort << " failed," << ec.message());
		}
	}
	else
	{
		LOG_WARN(this->name() << " ip or port error");	
	}

	
}

// 断开远端子服务器
void IcsWebClient::handleDisconnectRemote(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	ShortString remoteID;
	request >> remoteID;
	request.assertEmpty();

	auto conn = m_localServer.findRemoteProxy(remoteID);
	if (nullptr!=conn)
	{
		conn->do_error();
	}

}

// 转发到remote对应终端
void IcsWebClient::handleRemoteForward(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	// 消息结构：企业ID 网关ID 消息ID 消息体内容(请求ID 文件ID)
	ShortString enterpriseName;
	ShortString gwid;
	uint16_t messageID;
	uint32_t requestID;
	request >> enterpriseName >> gwid >> messageID >> requestID;
	request.rewind();

	if (!gwid.empty())
	{
		// 根据企业ID找到对应链接
		auto conn = m_localServer.findRemoteProxy(enterpriseName);
		if (conn)
		{
			try {
				conn->dispatch(request);
			}
			catch (IcsException& ex)
			{
				LOG_ERROR("forward to remote " << enterpriseName << "'s terminal " << gwid << " error:" << ex.message());
			}
		}
		else
		{
			// 转发失败结果记录到数据库
			LOG_ERROR("can't find enterpirse:" << gwid);

			OtlConnectionGuard connection(g_database);
			otl_stream s(1, "{ call sp_web_command_status(:requestID<int,in>,:msgID<int,in>,:stat<int,in>) }", connection.connection());
			s << (int)requestID << (int)messageID << (int)0;
		}
		
	}
}


//---------------------------ics remote proxy client---------------------------//
IcsRemoteProxyClient::IcsRemoteProxyClient(IcsLocalServer& localServer, socket&& s, std::string remoteID)
	: _baseType(localServer, std::move(s), "RemoteProxy")
	, m_enterpriseID(remoteID)
	, m_isLegal(false)
{
}

IcsRemoteProxyClient::~IcsRemoteProxyClient()
{
	
}


// 处理底层消息
void IcsRemoteProxyClient::handle(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	auto id = request.getHead()->getMsgID();
	if (id != C2C_auth_response_0x4002 && !m_isLegal)
	{
		throw IcsException("must response authrize message at first step");
	}
	switch (id)
	{
	case MessageId::C2C_auth_response_0x4002:
		handleAuthResponse(request, response);
		break;

	case MessageId::C2C_forward_response_0x4005:
		handleForwardResponse(request, response);
		break;

	case MessageId::C2C_terminal_onoff_line_0x4006:
		handleOnoffLine(request, response);
		break;

	case MessageId::C2C_forward_to_ics_0x4007:
		handleTerminalMessage(request, response);
		break;

	case MessageId::C2C_heartbeat_0x4008:	// ignore
		break;

	default:
//		throw IcsException("unknow RemoteProxy message id=0x%04x", (uint16_t)id);
		LOG_DEBUG("unknow RemoteProxy message id="<<(uint16_t)id);
		break;
	}
}

// 处理平层消息
void IcsRemoteProxyClient::dispatch(ProtocolStream& request) throw(IcsException, otl_exception)
{
	// 消息结构：企业ID 网关ID 消息ID 消息体内容(请求ID 文件ID)
	ProtocolStream response(ProtocolStream::OptType::writeType, g_memoryPool.get());
	response.initHead(MessageId::C2C_forward_to_terminal_0x4004, false);

	ShortString entepriseID;
	uint16_t messageID;

	request >> entepriseID;
	/// 跳过网关ID
	request.moveForward<ShortString>();
	request >> messageID;

	/// 若升级消息时需要提前查找文件路径放到该消息末尾处
	if (messageID == MessageId::C2T_upgrade_request_0x0201)
	{
		uint32_t fileid;

		/// 跳过请求ID,取出文件ID
		request.moveForward<uint32_t>();
		request >> fileid;

		std::string filepath;

		{
			// 从数据库中查询该文件ID对应的文件名
			// 转发结果记录到数据库
			OtlConnectionGuard connection(g_database);
			otl_stream s(1, "SELECT FILE_PATH FROM b_subComm_file_t WHERE FILE_ID=:fileid<int,in> AND ENTERPRISE_CODE=:entId<char[32],in>; ", connection.connection());
			s << (int)fileid << entepriseID;

			s >> filepath;
		}

		if (filepath.empty())
		{
			throw IcsException("can't find the file of the fileid(%d) for enterprise(%s)", fileid, entepriseID.c_str());
		}

		std::string tmp;
		request.rewind();
		request.moveForward<ShortString>();

		request >> tmp;
		response << tmp;	//  网关ID

		request >> messageID;
		response << messageID;	// 消息ID
		
		response << filepath;	// 文件全路径

		response << request;	// 剩余消息
	}
	else
	{
		request.rewind();
		request.moveForward<ShortString>();
		response << request;
	}
	_baseType::_baseType::trySend(response);
}

// 出错
void IcsRemoteProxyClient::error() throw()
{
	if (!m_enterpriseID.empty())
	{
		m_localServer.removeRemotePorxy(m_enterpriseID);
		if (m_isLegal)
		{
			OtlConnectionGuard connGuard(g_database);
			otl_stream s(1
				, "{ call sp_remote_proxy_onoff_line(:ent<char[33],in>,2,'',0) }"
				, connGuard.connection());
			s << m_enterpriseID;
		}
		m_enterpriseID.clear();
	}
}

// 请求验证中心身份
void IcsRemoteProxyClient::requestAuthrize()
{
	ProtocolStream request(ProtocolStream::OptType::writeType, g_memoryPool.get());
	request.initHead(MessageId::C2C_auth_request1_0x4001, false);

	// 取当前时间点
	std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

	// 加密该数据
	ics::encrypt(&t, sizeof(t));

	request << t;


	// 发送给远端通信服务器
	trySend(request);
	
	LOG_DEBUG("request proxy server authrize");
}

// 发送心跳消息
void IcsRemoteProxyClient::sendHeartbeat()
{
	ProtocolStream request(ProtocolStream::OptType::writeType, g_memoryPool.get());
	request.initHead(MessageId::C2C_heartbeat_0x4008, true);

	// 发送给远端通信服务器
	trySend(request);

	LOG_DEBUG("send heartbeat to proxy server");
}

// 查找远程ID对应的本地ID
const string& IcsRemoteProxyClient::findLocalID(const string& remoteID)
{
	auto& localId = m_remoteIdToDeviceIdMap[remoteID];
	// 不存在时从数据库中查询
	if (localId.empty())
	{
		OtlConnectionGuard connGuard(g_database);
		otl_stream s(1
			, "SELECT t.MONITORING_POINT_CODE FROM b_monitoring_point_t t WHERE t.GW_ID =:id<char[33]> AND t.STATUS = 0 LIMIT 1"
			, connGuard.connection());
		s << remoteID;
		s >> localId;
		if (localId.empty())
		{
			LOG_ERROR(name() << " the remote id=" << remoteID << " don't have local id");
		}
	}
	return localId;
}

// 处理认证请求结果
void IcsRemoteProxyClient::handleAuthResponse(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	std::time_t t1, t2, t3 = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	request >> t1 >> t2;

	request.assertEmpty();

	// 解密该数据
	ics::encrypt(&t1, sizeof(t1));
	auto interval = t3 - t1;
	if (interval < 5)	// 回复时间在5秒内,认证成功
	{
		LOG_DEBUG("authrize success, interval=" << interval);

		// 链接成功记录到数据库
		OtlConnectionGuard connGuard(g_database);
		otl_stream s(1
			, "{ call sp_remote_proxy_onoff_line(:ent<char[33],in>,1,:ip<char[16],in>,:port<int,in>) }"
			, connGuard.connection());
		s << m_enterpriseID << m_localServer.getWebIp() << m_localServer.getWebPort();

		m_isLegal = true;
		response.initHead(MessageId::C2C_auth_request2_0x4003, false);
		response << t2;

		m_localServer.addRemotePorxy(m_enterpriseID, shared_from_this());
	}
	else
	{
		throw IcsException("authrize failed, interval=%d", interval);
	}
}

// 代理服务器转发结果
void IcsRemoteProxyClient::handleForwardResponse(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	// 网关ID 消息ID 请求ID 结果 原因
	ShortString gwid, reason;
	uint32_t requestID;
	uint16_t messageID;
	uint8_t result;

	request >> gwid >> messageID >> requestID >> result;

	// 记录转发结果到数据库

	OtlConnectionGuard connection(g_database);
	otl_stream s(1, "{ call sp_webcmd_to_remote_proxy(:enterpriseID<char[32],in>,:requestID<int,in>,:msgID<int,in>,:stat<int,in>,:info<char[256],in>) }", connection.connection());
	s << m_enterpriseID << (int)requestID << (int)messageID << (int)result;

	if (result != 0) // 失败
	{
		request >> reason;
		s << reason;
		LOG_ERROR("forward to gwid=" << gwid << " failed,message id=" << messageID << ",request id=" << requestID << ",reason=" << reason);
	}
	else
	{
		s << "";
	}
	request.assertEmpty();
}

// 代理服务器上下线消息
void IcsRemoteProxyClient::handleOnoffLine(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	ShortString gwid;
	uint16_t devKind;
	uint8_t status;
	request >> gwid >> devKind >> status;
	request.assertEmpty();

	LOG_DEBUG(gwid << (status == 0 ? " online" : " offline"));

	OtlConnectionGuard connGuard(g_database);
	otl_stream s(1
		, "{ call sp_remote_terminal_onoff_line(:entID<char[32],in>,:gwid<char[33],in>,:devKind<int,in>,:stat<int,in>) }"
		, connGuard.connection());
	s << m_enterpriseID << gwid << (int)devKind << (int)status;
}

// 代理服务器转发终端的消息
void IcsRemoteProxyClient::handleTerminalMessage(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	// 网关ID 消息ID
	ShortString remoteGwid;
	uint16_t msgid;

	request >> remoteGwid >> msgid;

	auto& monitorName = findLocalID(remoteGwid);
	// 不存在时从数据库中查询
	if (!monitorName.empty())
	{
		request.initHead((MessageId)msgid, false);
		_baseType::m_monitorID = monitorName;
		try {
			_baseType::handle(request, response);
		}
		catch (IcsException& ex)
		{
			LOG_ERROR("Handle the remote gwid '" << remoteGwid<<"' IcsException,"<<ex.message());
		}
		catch (otl_exception& ex)
		{
			LOG_ERROR("Handle the remote gwid '" << remoteGwid << "' otl_exception," << ex.msg);
		}
	}
}



//---------------------------ics local server---------------------------//
IcsLocalServer::IcsLocalServer(asio::io_service& ioService, const string& terminalAddr, std::size_t terminalMaxCount, const string& webAddr, std::size_t webMaxCount, const string& pushAddr)
	: m_ioService(ioService)
	, m_terminalTcpServer(ioService), m_terminalMaxCount(terminalMaxCount)
	, m_webTcpServer(ioService), m_webMaxCount(webMaxCount)
	, m_pushSystem(ioService, pushAddr)
{
	//清除所有的链接信息;
	clearConnectionInfo();

	m_onlineIP = g_configFile.getAttributeString("protocol", "onlineIP");
	m_onlinePort = g_configFile.getAttributeInt("protocol", "onlinePort");
	m_heartbeatTime = g_configFile.getAttributeInt("protocol", "heartbeat");

	m_timer.start();

	m_terminalTcpServer.init("center's terminal"
		, terminalAddr
		, [this](socket&& s)
		{					
			ConneciontPrt conn = std::make_shared<IcsTerminalClient>(*this, std::move(s));
			conn->start();	// 投递读写事件
			connectionTimeoutHandler(conn); // 注册连接超时定时器
		});

	m_webTcpServer.init("center's web"
		, webAddr
		, [this](socket&& s)
		{
			ConneciontPrt conn = std::make_shared<IcsWebClient>(*this, std::move(s));
			conn->start();
			connectionTimeoutHandler(conn);
		});

	// 数据库记录该服务器地址 sp_server_onoff_line
	{
		OtlConnectionGuard connGuard(g_database);
		otl_stream s(1
			, "{ call sp_server_onoff_line(:ip<char[16],in>,:port<int,in>,:stat<int,in>) }"
			, connGuard.connection());
		s << m_onlineIP << m_onlinePort << (int)1;
	}
}

IcsLocalServer::~IcsLocalServer()
{
	// 服务器下线
	{
		OtlConnectionGuard connGuard(g_database);
		otl_stream s(1
			, "{ call sp_server_onoff_line(:ip<char[16],in>,:port<int,in>,:stat<int,in>) }"
			, connGuard.connection());
		s << m_onlineIP << m_onlinePort << (int)2;
	}

	// 停止tcp服务
	m_terminalTcpServer.stop();
	m_webTcpServer.stop();

	// 清除链接信息
	{
		std::lock_guard<std::mutex> lock(m_terminalConnMapLock);
		m_terminalConnMap.clear();
	}
	{
		std::lock_guard<std::mutex> lock(m_proxyConnMapLock);
		m_proxyConnMap.clear();
	}

	clearConnectionInfo();
}

/// 开启事件循环
void IcsLocalServer::start()
{
	
}

/// 停止事件
void IcsLocalServer::stop()
{

}

/// 添加已认证终端对象
void IcsLocalServer::addTerminalClient(const string& gwid, ConneciontPrt conn)
{
	std::lock_guard<std::mutex> lock(m_terminalConnMapLock);
	auto& oldConn = m_terminalConnMap[gwid];
	if (oldConn)
	{
		LOG_WARN(gwid << " from " << oldConn->name() << " is replaced by " << conn->name());
		oldConn->replaced();
	}
	oldConn = conn;
}

/// 移除已认证终端对象
void IcsLocalServer::removeTerminalClient(const string& conID)
{
	std::lock_guard<std::mutex> lock(m_terminalConnMapLock);
	m_terminalConnMap.erase(conID);
}

/// 查询终端链接
IcsLocalServer::ConneciontPrt IcsLocalServer::findTerminalClient(const string& conID)
{
	std::lock_guard<std::mutex> lock(m_proxyConnMapLock);
	auto it = m_terminalConnMap.find(conID);
	if (it != m_terminalConnMap.end())
	{
		return it->second;
	}
	else
	{
		return nullptr;
	}
}

/// 添加远程代理服务器对象
void IcsLocalServer::addRemotePorxy(const string& remoteID, ConneciontPrt conn)
{
//	keepHeartbeat(conn);
	std::lock_guard<std::mutex> lock(m_proxyConnMapLock);
	m_proxyConnMap[remoteID] = conn;
}

/// 移除远程代理服务器
void IcsLocalServer::removeRemotePorxy(const string& remoteID)
{
	std::lock_guard<std::mutex> lock(m_proxyConnMapLock);
	m_proxyConnMap.erase(remoteID);
}

/// 查询远端代理服务器
IcsLocalServer::ConneciontPrt IcsLocalServer::findRemoteProxy(const string& remoteID)
{
	std::lock_guard<std::mutex> lock(m_proxyConnMapLock);
	auto it = m_proxyConnMap.find(remoteID);
	if (it != m_proxyConnMap.end())
	{
		return it->second;
	}
	else
	{
		return nullptr;
	}
}

/// 初始化数据库连接信息
void IcsLocalServer::clearConnectionInfo()
{

		OtlConnectionGuard connGuard(g_database);
		otl_stream s(1
			, "{ call sp_clear_connection_info(:ip<char[32],in>,:port<int,in>) }"
			, connGuard.connection());
		s << m_onlineIP << m_onlinePort;

}

/// 连接超时处理
void IcsLocalServer::connectionTimeoutHandler(ConneciontPrt conn)
{
	m_timer.add(m_heartbeatTime,
		std::bind([conn, this](){
		if (conn->timeout())
		{
			this->connectionTimeoutHandler(conn);
		}
	}));
}

/// 保持代理服务器心跳
void IcsLocalServer::keepHeartbeat(ConneciontPrt conn)
{
	auto proxy = std::dynamic_pointer_cast<IcsRemoteProxyClient>(conn);
	m_timer.add(m_heartbeatTime*2,
		std::bind([proxy, this](){
		proxy->sendHeartbeat();
		keepHeartbeat(proxy);
	}));
}


}