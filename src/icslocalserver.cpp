

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
	if (!_baseType::m_replaced && !m_connName.empty())
	{
		OtlConnectionGuard connGuard(g_database);

		otl_stream s(1
			, "{ call sp_offline(:id<char[33],in>,:ip<char[17],in>,:port<int,in>) }"
			, connGuard.connection());

		s << m_connName << m_localServer.m_onlineIP << m_localServer.m_onlinePort;
	}
}

// 处理底层消息
void IcsTerminalClient::handle(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	switch (request.getHead()->getMsgID())
	{
	case T2C_auth_request:
		handleAuthRequest(request, response);
		break;

	case T2C_heartbeat:
		handleHeartbeat(request, response);
		break;

	case T2C_std_status_report:
		handleStdStatusReport(request, response);
		break;

	case T2C_def_status_report:
		handleDefStatusReport(request, response);
		break;

	case T2C_event_report:
		handleEventsReport(request, response);
		break;

	case T2C_bus_report:
		handleBusinessReport(request, response);
		break;

	case T2C_gps_report:
		handleGpsReport(request, response);
		break;

	case T2C_datetime_sync_request:
		handleDatetimeSync(request, response);
		break;

	case T2C_log_report:
		handleLogReport(request, response);
		break;

	default:
		throw IcsException("unknown terminal message id = 0x%04x ", (uint16_t)request.getHead()->getMsgID());
		break;
	}
}

// 处理平层消息
void IcsTerminalClient::dispatch(ProtocolStream& request) throw(IcsException, otl_exception)
{
	ShortString terminalName;
	uint16_t messageID;
	uint32_t requestID;
	request >> terminalName >> messageID >> requestID;

	if (messageID <= MessageId::T2C_min || messageID >= MessageId::T2C_max)
	{
		throw IcsException("dispatch message=%d is not one of T2C", messageID);
	}
	request.moveBack(sizeof(requestID));

	// 记录该请求ID转发结果


	// 发送到该链接对端
	ProtocolStream response(g_memoryPool);
	response.initHead((MessageId)messageID, false);
	response << request;

	_baseType::trySend(response);
}

// 终端认证
void IcsTerminalClient::handleAuthRequest(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	if (!m_connName.empty())
	{
		LOG_DEBUG(m_connName << " ignore repeat authrize message");
		response.initHead(MessageId::C2T_auth_response, request.getHead()->getSendNum());
		response << ShortString("ok") << (uint16_t)10;
		return;
	}

	// auth info
	string gwId, gwPwd, extendInfo;

	request >> gwId >> gwPwd >> m_deviceKind >> extendInfo;

	request.assertEmpty();

	OtlConnectionGuard connGuard(g_database);

	otl_stream authroizeStream(1,
		"{ call sp_authroize(:gwid<char[33],in>,:pwd<char[33],in>,@ret,@id,@name) }",
		connGuard.connection());

	authroizeStream << gwId << gwPwd;
	authroizeStream.close();

	otl_stream getStream(1, "select @ret :#<int>,@id :#<char[32]>,@name :#name<char[32]>", connGuard.connection());

	int ret = 2;
	string id, name;

	getStream >> ret >> id >> name;

	response.initHead(MessageId::C2T_auth_response, false);

	if (ret == 0)	// 成功
	{
		m_connName = std::move(id); // 保存检测点id

		otl_stream onlineStream(1
			, "{ call sp_online(:id<char[33],in>,:ip<char[16],in>,:port<int,in>) }"
			, connGuard.connection());

		onlineStream << m_connName << m_localServer.m_onlineIP << m_localServer.m_onlinePort;

		response << ShortString("ok") << m_localServer.m_heartbeatTime;

		m_localServer.addTerminalClient(m_connName, std::unique_ptr<IcsConnection<icstcp>>(this));

		LOG_INFO("terminal " << m_connName << " created on " << this->name());

		this->setName(m_connName + "@" + this->name());
	}
	else
	{
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
			"{ call sp_status_standard(:id<char[33],in>,:devFlag<int,in>,:devStat<char[512],in>,:cheatFlag<int,in>,:cheatStat<char[512],in>,:zeroPoint<float,in>,:recvTime<timestamp,in>) }"
			, connGuard.connection());

		o << m_connName << (int)device_ligtht << device_status << (int)cheat_ligtht << cheat_status << zero_point << recv_time;
	}
	// 其它
	else
	{
		LOG_WARN(m_connName << "recv unknown standard status type:" << status_type);
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

		eventStream << m_connName << (int)m_deviceKind << (int)event_id << (int)event_type << event_value << event_time << recv_time;

		// 发送给推送服务器: 监测点ID 发生时间 事件编号 事件值
		ProtocolStream pushStream(g_memoryPool);
		pushStream << m_connName << m_deviceKind << event_time << event_id << event_value;

		m_localServer.m_pushSystem.send(pushStream);
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
		response.initHead(MessageId::MessageId_min, false);
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

		s << m_connName << (int)business_no << cargo_num << vehicle_num
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

			s << m_connName << business_no << (int)amount << total_weight << single_weighet << report_time << recv_time;
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
			, "{ call `ics_highway`.sp_business_expressway(:id<char[33],in>,:num<int,in>,:weight<int,in>,:speed<float,in>,:axleNum<int,in>,:axleStr<char[256],in>,:typeNum<int,in>,:typeStr<char[256],in>,:reportTime<timestamp,in>,:recvTime<timestamp,in>) }"
			, connGuard.connection());

		s << m_connName << (int)business_no << (int)total_weight << speed*0.1 << (int)axle_num << axle_str << (int)type_num << type_str << report_time << recv_time;
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

		s << m_connName << (int)business_no << report_time << recv_time
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

	s << m_connName << (int)postionFlag.longitude_flag << (int)longitude << (int)postionFlag.latitude_flag << (int)latitude << (int)postionFlag.signal << (int)height << (int)speed;
}

// 终端回应参数查询
void IcsTerminalClient::handleParamQueryResponse(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	uint32_t request_id;	// 请求id
	uint16_t param_count;	// 参数数量

	uint8_t net_id = 0;		//	子网编号
	uint16_t param_id = 0;	//	参数编号
	uint8_t param_type = 0;	//	参数值类型
	string param_value;		//	参数值

	request >> request_id >> param_count;

	// 		for (uint16_t i = 0; i<param_count; i++)
	// 		{
	// 		request >> net_id >> param_id >> param_type >> param_value;
	// 		// 存入数据库
	// 
	// 		}

}

// 终端主动上报参数修改
void IcsTerminalClient::handleParamAlertReport(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	IcsDataTime alert_time;	//	修改时间
	uint16_t param_count;	// 参数数量

	uint8_t net_id = 0;		//	子网编号
	uint16_t param_id = 0;	//	参数编号
	uint8_t param_type = 0;	//	参数值类型
	string param_value;		//	参数值

	request >> alert_time >> param_count;

	for (uint16_t i = 0; i < param_count; i++)
	{
		request >> net_id >> param_id >> param_type >> param_value;

		// 存入数据库

	}
}

// 终端回应参数修改
void IcsTerminalClient::handleParamModifyResponse(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	uint32_t request_id;	// 请求id
	uint16_t param_count;	// 参数数量

	uint8_t net_id = 0;		//	子网编号
	uint16_t param_id = 0;	//	参数编号
	string alert_value;		//	修改结果

	request >> request_id >> param_count;

	for (uint16_t i = 0; i < param_count; i++)
	{
		request >> net_id >> param_id >> alert_value;
	}
}

// 终端发送时钟同步请求
void IcsTerminalClient::handleDatetimeSync(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	IcsDataTime dt1, dt2;
	request >> dt1;
	request.assertEmpty();

	getIcsNowTime(dt2);

	response.initHead(MessageId::MessageId_min, m_send_num++);
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

	s << m_connName << status_time << (int)log_level << log_value;
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

	s << m_connName << int(request_id) << reason;
}

// 终端接收升级请求
void IcsTerminalClient::handleAgreeUpgrade(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	uint32_t request_id;	// 文件id
	request >> request_id;
	request.assertEmpty();

	OtlConnectionGuard connGuard(g_database);
	otl_stream s(1
		, "{ call sp_upgrade_accept(:id<char[33],in>,:reqID<int,in>) }"
		, connGuard.connection());

	s << m_connName << int(request_id);
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
		, "{ call sp_upgrade_set_progress(:F1<int,in>,:F2<int,in>,@stat) }"
		, connGuard.connection());

	s << m_connName << (int)request_id << (int)received_size;

	otl_stream queryResutl(1, "select @stat :#<int>", connGuard.connection());

	int result = 0;

	queryResutl >> result;

	// 查找文件
	auto fileInfo = FileUpgradeManager::getInstance()->getFileInfo(file_id);


	// 查看是否已取消升级
	if (result == 0 && fileInfo)	// 正常状态且找到该文件
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

		response << file_id << request_id << fragment_offset << fragment_length;

		response.append((char*)fileInfo->file_content + fragment_offset, fragment_length);

	}
	else	// 无升级事务
	{
		response.initHead(MessageId::T2C_upgrade_not_found, request.getHead()->getAckNum());
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
	uint32_t request_id;	// 文件id

	request >> request_id;

	request.assertEmpty();

	OtlConnectionGuard connGuard(g_database);
	otl_stream s(1
		, "{ call sp_upgrade_cancel_ack(:F1<int,in>) }"
		, connGuard.connection());

	s << (int)request_id;
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
	case MessageId::W2C_send_to_terminal:
		handleForward(request, response);
		break;

	case MessageId::W2C_connect_remote_request:
		handleConnectRemote(request, response);
		break;

	case MessageId::W2C_disconnect_remote:
		handleDisconnectRemote(request, response);
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

// 转发到对应终端
void IcsWebClient::handleForward(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	ShortString terminalName;
	uint16_t messageID;
	uint32_t requestID;
	request >> terminalName >> messageID >> requestID;
	request.rewindReadPos();

	if (!terminalName.empty())
	{
		// 根据终端ID转发该消息
		bool result = m_localServer.sendToTerminalClient(terminalName, request);

		// 转发结果记录到数据库
		{
			OtlConnectionGuard connection(g_database);
			otl_stream s(1, "{ call sp_web_command_status(:requestID<int,in>,:msgID<int,in>,:stat<int,in>) }", connection.connection());
			s << (int)requestID << (int)messageID << (result ? 0 : 1);
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

	// 从数据库中查询该远端地址
	OtlConnectionGuard connGuard(g_database);

	otl_stream queryStream(1
//		, "{ call sp_query_remote_server(:id<char[33],in>) }"
		, "SELECT t.SERVER_IP ip,t.SERVER_POTR PORT FROM b_subComm_connection_t t WHERE t.ENTERPRISE_CODE=:id<char[33]>"
		, connGuard.connection());

	queryStream << remoteID;

	string remoteIP;
	int remotePort;

	queryStream >> remoteIP >> remotePort;


	// 回应web连接结果:0-成功,1-失败
	response << remoteID;

	if (remoteIP.empty() || remotePort == 0)
	{
		response << (uint8_t)1;
		LOG_WARN(this->name() << " ip or port error");	
		return;
	}

	// 连接该远端
	bool connectResult = true;
	asio::ip::tcp::socket remoteSocket(m_localServer.m_ioService);
	asio::ip::tcp::endpoint endpoint(asio::ip::address::from_string(remoteIP), remotePort);
	asio::error_code ec;
	remoteSocket.connect(endpoint, ec);

	if (!ec)
	{
		auto conn = new IcsRemoteProxyClient(m_localServer, std::move(remoteSocket), remoteID);
		conn->start();
		conn->requestAuthrize();
//		m_localServer.addRemotePorxy(remoteID, IcsLocalServer::ConneciontPrt(conn));

		response << (uint8_t)0;
	}
	else
	{
		response << (uint8_t)1;
	}
}

// 断开远端子服务器
void IcsWebClient::handleDisconnectRemote(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	ShortString remoteID;
	request >> remoteID;
	request.assertEmpty();

	/*
	// 从数据库中查询该远端地址
	OtlConnectionGuard connGuard(g_database);

	otl_stream queryStream(1
		, "{ call sp_query_remote_server(:id<char[33],in>,@ip,@port) }"
		, connGuard.connection());

	queryStream << remoteID;

	otl_stream getStream(1, "select @ip :#<char[32]>,@port :#<int>", connGuard.connection());

	string remoteIP;
	int remotePort;

	getStream >> remoteIP >> remotePort;
	*/

	// 断开该远端
	bool connectResult = true;

	m_localServer.removeRemotePorxy(remoteID);

	// 回应web连接结果:0-成功,1-失败
	response << remoteID << (uint8_t)(connectResult ? 0 : 1);
}



//---------------------------ics remote proxy client---------------------------//
IcsRemoteProxyClient::IcsRemoteProxyClient(IcsLocalServer& localServer, socket&& s, std::string remoteID)
	: _baseType(localServer, std::move(s), "RemoteProxy")
	, m_enterpriseID(remoteID)
{
}

IcsRemoteProxyClient::~IcsRemoteProxyClient()
{
//	m_localServer.removeRemotePorxy(m_enterpriseID);
}

std::unordered_map<std::string, std::string> IcsRemoteProxyClient::s_remoteIdToDeviceIdMap;
std::mutex IcsRemoteProxyClient::s_idMapLock;

// 处理底层消息
void IcsRemoteProxyClient::handle(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	switch (request.getHead()->getMsgID())
	{
	case MessageId::T2T_auth_response:
		handleAuthResponse(request, response);
		break;

	case MessageId::T2T_forward_msg:
		handleForwardMessage(request, response);
		break;

	default:
//		throw IcsException("unknow RemoteProxy message id=0x%04x", (uint16_t)request.getHead()->getMsgID());
		LOG_DEBUG("unknow RemoteProxy message id="<<(uint16_t)request.getHead()->getMsgID());
		break;
	}
}

// 处理平层消息
void IcsRemoteProxyClient::dispatch(ProtocolStream& request) throw(IcsException, otl_exception)
{
	throw IcsException("IcsRemoteProxyClient never dispatch");
}

// 请求验证中心身份
void IcsRemoteProxyClient::requestAuthrize()
{
	ProtocolStream request(g_memoryPool);

	// 取当前时间点
	std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

	// 加密该数据
	ics::encrypt(&t, sizeof(t));

	request.initHead(MessageId::T2T_auth_request, false);
	request << t;


	// 发送给远端通信服务器
	_baseType::trySend(request);
	
	LOG_DEBUG("request proxy server authrize");
}

// 处理认证请求结果
void IcsRemoteProxyClient::handleAuthResponse(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	std::time_t t1,t3 = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	request >> t1;

	request.assertEmpty();

	// 解密该数据
	ics::encrypt(&t1, sizeof(t1));

	if (t3 - t1 < 5)	// 回复时间在5秒内,认证成功
	{
		LOG_DEBUG("authrize success, interval=" << t3 - t1);
	}
	else
	{
		LOG_DEBUG("authrize failed, interval=" << t3 - t1);
	}

}

// 处理代理服务器转发的消息
void IcsRemoteProxyClient::handleForwardMessage(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	// 在消息体开始处取出消息ID、终端编号(gwid)
	uint16_t msgid;
	ShortString monitorID;
	request >> msgid >> monitorID;

	auto& localId = s_remoteIdToDeviceIdMap[monitorID];
	// 不存在时从数据库中查询
	if (localId.empty())
	{
		OtlConnectionGuard connGuard(g_database);
		otl_stream s(1
			, "SELECT t.MONITORING_POINT_CODE FROM b_monitoring_point_t t WHERE t.GW_ID =:id<char[33]> AND t.STATUS = 0 LIMIT 1"
			, connGuard.connection());
		s << monitorID;
		s >> localId;
		if (localId.empty())
		{
			LOG_ERROR(name() << " the remote id=" << monitorID << " don't have local id,drop the message");
			return;
		}
	}

	request.initHead((MessageId)msgid, true);
	_baseType::m_connName = localId;
	_baseType::handle(request, response);
}

//---------------------------ics local server---------------------------//
IcsLocalServer::IcsLocalServer(asio::io_service& ioService, const string& terminalAddr, std::size_t terminalMaxCount, const string& webAddr, std::size_t webMaxCount, const string& pushAddr)
	: m_ioService(ioService)
	, m_terminalTcpServer(ioService), m_terminalMaxCount(terminalMaxCount)
	, m_webTcpServer(ioService), m_webMaxCount(webMaxCount)
	, m_pushSystem(ioService, pushAddr)
{
	m_onlineIP = g_configFile.getAttributeString("protocol", "onlineIP");
	m_onlinePort = g_configFile.getAttributeInt("protocol", "onlinePort");
	m_heartbeatTime = g_configFile.getAttributeInt("protocol", "heartbeat");

	m_terminalTcpServer.init("center's terminal"
		, terminalAddr
		, [this](socket&& s)
		{					
			std::shared_ptr<IcsConnection<icstcp>> conn(new IcsTerminalClient(*this, std::move(s)));
			conn->start();

			m_timer.add(m_heartbeatTime, [conn, this](){
				return conn->timeout(m_heartbeatTime);
				});
		});

	m_webTcpServer.init("center's web"
		, webAddr
		, [this](socket&& s)
		{
			std::shared_ptr<IcsConnection<icstcp>> conn(new IcsWebClient(*this, std::move(s)));
			conn->start();

			m_timer.add(m_heartbeatTime, [conn, this](){
				return conn->timeout(m_heartbeatTime);
			});

		});

	m_timer.start();
}

IcsLocalServer::~IcsLocalServer()
{
	m_timer.stop();
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
void IcsLocalServer::addTerminalClient(const string& conID, ConneciontPrt conn)
{
	std::lock_guard<std::mutex> lock(m_terminalConnMapLock);
	auto& oldConn = m_terminalConnMap[conID];
	if (oldConn)
	{
		LOG_WARN(conID << " from " << oldConn->name() << " is replaced by " << conn->name());
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

/// 发送数据到终端对象
bool IcsLocalServer::sendToTerminalClient(const string& conID, ProtocolStream& request)
{
	bool ret = false;
	std::lock_guard<std::mutex> lock(m_terminalConnMapLock);
	auto& oldConn = m_terminalConnMap[conID];
	if (oldConn)
	{
		try 
		{
			oldConn->dispatch(request);
			ret = true;
		}
		catch (IcsException& ex)
		{
			LOG_ERROR("send to " << conID << " failed,inner info: " << ex.message());
		}
		catch (otl_exception& ex)
		{
			LOG_ERROR("send to " << conID << " failed,db info: " << ex.msg);
		}
	}

	return ret;
}

/// 添加远程代理服务器对象
void IcsLocalServer::addRemotePorxy(const string& remoteID, ConneciontPrt conn)
{
	std::lock_guard<std::mutex> lock(m_proxyConnMapLock);
	m_proxyConnMap[remoteID] = std::move(conn);
}

/// 移除远程代理服务器
void IcsLocalServer::removeRemotePorxy(const string& remoteID)
{
	std::lock_guard<std::mutex> lock(m_proxyConnMapLock);
	m_proxyConnMap.erase(remoteID);
}

}