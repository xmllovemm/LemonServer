
#include "icsproxyserver.hpp"
#include "util.hpp"
#include "downloadfile.hpp"

extern ics::IcsConfig g_configFile;


namespace ics {

//---------------------------IcsProxyTerminalClient---------------------------//
IcsProxyTerminalClient::IcsProxyTerminalClient(IcsPorxyServer& localServer, socket&& s)
	: _baseType(std::move(s), nullptr)
	, m_proxyServer(localServer)
{
}

IcsProxyTerminalClient::~IcsProxyTerminalClient()
{
}

// 处理底层消息
void IcsProxyTerminalClient::handle(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	auto id = request.getHead()->getMsgID();
	if (id != T2C_auth_request && m_connName.empty())
	{
		throw IcsException("must authrize at first step");
	}
	switch (id)
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
		LOG_WARN(this->name() << " unknown terminal message id = " << (uint16_t)id);
//		throw IcsException("%s recv unknown terminal message id = %04x ",_baseType::m_name.c_str(), (uint16_t)id);
		return;
	}

}

// 处理平层消息
void IcsProxyTerminalClient::dispatch(ProtocolStream& request) throw(IcsException, otl_exception)
{

	uint32_t requestID;
	request >> requestID;

	
	request.moveBack(sizeof(requestID));

	// 记录该请求ID转发结果
	uint32_t fileid;
	ShortString filename;

	// 加载文件不成功时不再转发给终端
	if (!FileUpgradeManager::getInstance()->loadFileInfo(fileid, filename))
	{
		throw IcsException("can't load file(fileid=%d, %s)", fileid, filename.c_str());
	}

	// 发送到该链接对端
	ProtocolStream forward(ProtocolStream::OptType::writeType, g_memoryPool.get());
	//forward.initHead()
	forward << request;

	_baseType::trySend(forward);
}

// 出错处理
void IcsProxyTerminalClient::error() throw()
{
	// 已认证设备离线
	if (!_baseType::m_replaced && !m_connName.empty())
	{
		m_proxyServer.removeTerminalClient(m_connName);
		onoffLineToIcsCenter(1);	// 通知ICS中心
		m_connName.clear();
	}
}

/// 转发到ICS中心
void IcsProxyTerminalClient::forwardToIcsCenter(ProtocolStream& request)
{
	ProtocolStream forward(ProtocolStream::OptType::writeType, g_memoryPool.get());

	request.rewind();
	forward.initHead(MessageId::C2C_forward_to_ics, false);
	forward << m_connName << (uint16_t)request.getHead()->getMsgID() << request;

	m_proxyServer.sendToIcsCenter(forward);
}

/// 上下线通知:status 0-online,1-offline
void IcsProxyTerminalClient::onoffLineToIcsCenter(uint8_t status)
{
	ProtocolStream forward(ProtocolStream::OptType::writeType, g_memoryPool.get());
	forward.initHead(MessageId::C2C_terminal_onoff_line, false);
	forward << m_connName << m_deviceKind << status;

	m_proxyServer.sendToIcsCenter(forward);
}

// 终端认证
void IcsProxyTerminalClient::handleAuthRequest(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
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


	response.initHead(MessageId::C2T_auth_response, false);

	if (1)	// 成功
	{
		m_connName = std::move(gwId); // 保存检测点id

		response << "ok" << m_proxyServer.getHeartbeatTime();

		m_proxyServer.addTerminalClient(m_connName, shared_from_this());

		LOG_INFO("terminal " << m_connName << " created on " << this->name());

		this->setName(m_connName + "@" + this->name());

		onoffLineToIcsCenter(0);
	}
	else
	{
		response << "failed";
	}
}

// 标准状态上报
void IcsProxyTerminalClient::handleStdStatusReport(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception, otl_exception)
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

		forwardToIcsCenter(request);
	}
	// 其它
	else
	{
		LOG_WARN(m_connName << "recv unknown standard status type:" << status_type);
	}
}

// 自定义状态上报
void IcsProxyTerminalClient::handleDefStatusReport(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
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
void IcsProxyTerminalClient::handleEventsReport(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	IcsDataTime event_time, recv_time;	//	事件发生时间 接收时间
	uint16_t event_count;	// 事件项个数

	uint16_t event_id = 0;	//	事件编号
	uint8_t event_type = 0;	//	事件值类型
	string event_value;		//	事件值

	getIcsNowTime(recv_time);

	request >> event_time >> event_count;

	/*
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

		m_proxyServer.m_pushSystem.send(pushStream);
	}
	

	request.assertEmpty();
	*/

	forwardToIcsCenter(request);	// 转发到中心
}

// 业务上报
void IcsProxyTerminalClient::handleBusinessReport(ProtocolStream& request, ProtocolStream& response)  throw(IcsException, otl_exception)
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

	/*
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
	*/
	
//	request.assertEmpty();

forwardToIcsCenter(request);	// 转发到中心
}

// 终端发送心跳到中心
void IcsProxyTerminalClient::handleHeartbeat(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{

}

// 终端发送时钟同步请求
void IcsProxyTerminalClient::handleDatetimeSync(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	IcsDataTime dt1, dt2;
	request >> dt1;
	request.assertEmpty();

	getIcsNowTime(dt2);

	response.initHead(MessageId::MessageId_min, m_send_num++);
	response << dt1 << dt2 << dt2;
}

// GPS上报
void IcsProxyTerminalClient::handleGpsReport(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
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


	forwardToIcsCenter(request);

}

// 终端上报日志
void IcsProxyTerminalClient::handleLogReport(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
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

	forwardToIcsCenter(request);	// 转发到中心
}

// 终端拒绝升级请求
void IcsProxyTerminalClient::handleDenyUpgrade(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	uint32_t request_id;	// 请求id
	string reason;	// 拒绝升级原因

	request >> request_id >> reason;

	request.assertEmpty();

	forwardToIcsCenter(request);	// 转发到中心
}

// 终端接收升级请求
void IcsProxyTerminalClient::handleAgreeUpgrade(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	uint32_t request_id;	// 文件id
	request >> request_id;
	request.assertEmpty();

	forwardToIcsCenter(request);	// 转发到中心
}

// 索要升级文件片段
void IcsProxyTerminalClient::handleRequestFile(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{

	uint32_t file_id, request_id, fragment_offset, received_size;

	uint16_t fragment_length;

	request >> file_id >> request_id >> fragment_offset >> fragment_length >> received_size;

	request.assertEmpty();

	// 设置升级进度(查询该请求id对应的状态)


	// 查找文件
	auto fileInfo = FileUpgradeManager::getInstance()->getFileInfo(file_id);


	// 查看是否已取消升级
	if (fileInfo)	// 正常状态且找到该文件
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

		forwardToIcsCenter(request);	// 转发到中心，记录升级进度
	}
	else	// 无升级事务/找不到文件
	{
		response.initHead(MessageId::C2T_upgrade_not_found, request.getHead()->getAckNum());
	}
}

// 升级文件传输结果
void IcsProxyTerminalClient::handleUpgradeResult(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	uint32_t request_id;	// 文件id
	string upgrade_result;	// 升级结果

	request >> request_id >> upgrade_result;

	request.assertEmpty();

	forwardToIcsCenter(request);	// 转发到中心
}

// 终端确认取消升级
void IcsProxyTerminalClient::handleUpgradeCancelAck(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	uint32_t request_id;	// 文件id

	request >> request_id;

	request.assertEmpty();

	forwardToIcsCenter(request);	// 转发到中心
}



//---------------------------IcsCenter---------------------------//
IcsCenter::IcsCenter(IcsPorxyServer& localServer, socket&& s)
	: _baseType(std::move(s), "IcsCenter")
	, m_proxyServer(localServer)
{
}

IcsCenter::~IcsCenter()
{

}

// 处理底层消息
void IcsCenter::handle(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	auto id = request.getHead()->getMsgID();
	switch (id)
	{
	case MessageId::C2C_auth_request1:
		handleAuthrize1(request, response);
		break;

	case MessageId::C2C_auth_request2:
		handleAuthrize2(request, response);
		break;

	case MessageId::C2C_forward_to_terminal:
		handleForwardToTermianl(request, response);
		break;

	case MessageId::C2C_heartbeat:	// ignore
		break;

	default:		
		throw IcsException("unknow message id=0x%04x", (uint16_t)id);
		break;
	}
}

// 处理平层消息
void IcsCenter::dispatch(ProtocolStream& request) throw(IcsException, otl_exception)
{
	this->trySend(request);
}

/// 中心认证请求1
void IcsCenter::handleAuthrize1(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	std::time_t t1, t2 = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	request >> t1;

	request.assertEmpty();

	response.initHead(MessageId::C2C_auth_response, false);
	response << t1 << t2;
}

/// 出错
void IcsCenter::error() throw()
{
	m_proxyServer.removeIcsCenterConn(shared_from_this());
}

/// 中心认证请求2
void IcsCenter::handleAuthrize2(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	std::time_t t2, t4 = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	request >> t2;

	request.assertEmpty();

	// 解密该数据
	ics::decrypt(&t2, sizeof(t2));

	auto interval = t4 - t2;
	if (interval < 5)	// 系统时间在3分钟内,认证成功
	{
		m_proxyServer.addIcsCenterConn(shared_from_this());

		LOG_DEBUG("ics center authrize success, interval=" << interval);
	}
	else
	{
		throw IcsException("ics center authrize failed, interval=", interval);
	}
}

/// 转发消息给终端
void IcsCenter::handleForwardToTermianl(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	ShortString terminalName;
	uint16_t messageID;
//	uint32_t requestID;
	request >> terminalName >> messageID;

	auto conn = m_proxyServer.findTerminalClient(terminalName);
	if (conn)
	{
		request.rewind();
		request.initHead((MessageId)messageID, false);
		conn->dispatch(request);
		response << (uint8_t)0;
	}
	else
	{
		response << (uint8_t)1 << "can't be found";
	}
}

//---------------------------IcsPorxyServer---------------------------//
IcsPorxyServer::IcsPorxyServer(asio::io_service& ioService
	, const string& terminalAddr, std::size_t terminalMaxCount
	, const string& icsCenterAddr, std::size_t icsCenterCount)
	: m_ioService(ioService)
	, m_terminalTcpServer(ioService), m_terminalMaxCount(terminalMaxCount)
	, m_icsCenterTcpServer(ioService), m_icsCenterMaxCount(icsCenterCount)
{
	m_heartbeatTime = g_configFile.getAttributeInt("protocol", "heartbeat");

	m_terminalTcpServer.init("remote's terminal"
		, terminalAddr
		, [this](socket&& s)
		{
			ConneciontPrt conn = std::make_shared<IcsProxyTerminalClient>(*this, std::move(s));
			conn->start();
			connectionTimeoutHandler(conn);
		});

	m_icsCenterTcpServer.init("remote's center"
		, icsCenterAddr
		, [this](socket&& s)
	{
		ConneciontPrt conn = std::make_shared<IcsCenter>(*this, std::move(s));
		conn->start();
		connectionTimeoutHandler(conn);
	});

	m_timer.start();
}

IcsPorxyServer::~IcsPorxyServer()
{
	m_terminalTcpServer.stop();
	m_icsCenterTcpServer.stop();
	m_timer.stop();
	{
		std::lock_guard<std::mutex> lock(m_terminalConnMapLock);
		m_terminalConnMap.clear();
	}
	{
		std::lock_guard<std::mutex> lock(m_icsCenterConnMapLock);
		m_icsCenterConnMap.clear();
	}
}

/// 添加已认证终端
void IcsPorxyServer::addTerminalClient(const string& conID, ConneciontPrt conn)
{
	std::lock_guard<std::mutex> lock(m_terminalConnMapLock);
	auto& c = m_terminalConnMap[conID];
	if (c)
	{
		LOG_WARN(conID << " from " << c->name() << " is replaced by " << conn->name());
		c->replaced();
	}
	c = conn;
}

/// 移除已认证终端
void IcsPorxyServer::removeTerminalClient(const string& conID)
{
	std::lock_guard<std::mutex> lock(m_terminalConnMapLock);
	m_terminalConnMap.erase(conID);
}

/// 查找链接终端
IcsPorxyServer::ConneciontPrt IcsPorxyServer::findTerminalClient(const string& conID)
{
	IcsPorxyServer::ConneciontPrt conn;
	{
		std::lock_guard<std::mutex> lock(m_terminalConnMapLock);
		auto it = m_terminalConnMap.find(conID);
		if (it != m_terminalConnMap.end())
		{
			conn = it->second;
		}
	}
	return conn;
}

/// 向终端发送数据
bool IcsPorxyServer::sendToTerminalClient(const string& conID, ProtocolStream& request)
{
	bool ret = false;
	ConneciontPrt conn;

	{
		std::lock_guard<std::mutex> lock(m_terminalConnMapLock);
		auto it = m_terminalConnMap.find(conID);
		if (it != m_terminalConnMap.end())
		{
			conn = it->second;
		}
	}

	if (conn)
	{
		try {
			conn->dispatch(request);
			ret = true;
		}
		catch (IcsException& ex)
		{
			LOG_ERROR("send to " << conID << " failed," << ex.message());
		}
	}
	
	return ret;
}

/// 添加ICS中心服务链接
void IcsPorxyServer::addIcsCenterConn(ConneciontPrt conn)
{
	std::lock_guard<std::mutex> lock(m_icsCenterConnMapLock);
	m_icsCenterConnMap.emplace(conn);
}

/// 移除ICS中心服务链接
void IcsPorxyServer::removeIcsCenterConn(ConneciontPrt conn)
{
	std::lock_guard<std::mutex> lock(m_icsCenterConnMapLock);
	m_icsCenterConnMap.erase(conn);
}

/// 向全部ICS中心发送数据
void IcsPorxyServer::sendToIcsCenter(ProtocolStream& request)
{
	std::lock_guard<std::mutex> lock(m_icsCenterConnMapLock);
	for (auto& it : m_icsCenterConnMap)
	{
		ProtocolStream message(request, g_memoryPool.get());
		it->dispatch(message);
	}
}

/// 
void IcsPorxyServer::connectionTimeoutHandler(ConneciontPrt conn)
{
	m_timer.add(m_heartbeatTime, 
		std::bind([conn, this](){
			if (conn->timeout())
			{
				this->connectionTimeoutHandler(conn);
			}
		}));
}

}