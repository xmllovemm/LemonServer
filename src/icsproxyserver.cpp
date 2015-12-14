
#include "icsproxyserver.hpp"

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
	auto msgid = request.getHead()->getMsgID();

	switch (msgid)
	{
	case T2C_auth_request:
		handleAuthRequest(request, response);
		break;

	case T2C_heartbeat:
		handleHeartbeat(request, response);
		break;

	case T2C_event_report:
		handleEventsReport(request, response);
		break;

	case T2C_bus_report:
		handleBusinessReport(request, response);
		break;

	case T2C_datetime_sync_request:
		handleDatetimeSync(request, response);
		break;


	default:
		LOG_WARN(_baseType::m_name << " recv unknown terminal message id = " << (uint16_t)msgid);
//		throw IcsException("%s recv unknown terminal message id = %04x ",_baseType::m_name.c_str(), request.getHead()->getMsgID());
		return;
	}

	if (msgid == T2C_event_report)
	{
		auto chunk = g_memoryPool.get();
		ProtocolStream forward(chunk);

//		request.rewindReadPos();
//		forward.initHead(MessageId::T2T_forward_msg, false);
//		forward << (uint16_t)msgid << m_connName << request;

		m_proxyServer.sendToIcsCenter(forward);
	}
}

// 处理平层消息
void IcsProxyTerminalClient::dispatch(ProtocolStream& request) throw(IcsException, otl_exception)
{
	ProtocolStream reinput(std::move(request));
	ShortString terminalName;
	uint16_t messageID;
	uint32_t requestID;
	reinput >> terminalName >> messageID >> requestID;

	if (messageID <= MessageId::T2C_min || messageID >= MessageId::T2C_max)
	{
		throw IcsException("dispatch message=%d is not one of T2C", messageID);
	}
//	reinput.seek(ProtocolStreamImpl::SeekPos::PosCurrent, -sizeof(requestID));

	// 记录该请求ID转发结果


	// 发送到该链接对端
	ProtocolStream response(g_memoryPool);
//	response.initHead((MessageId)messageID, false);
	response << reinput;

	_baseType::trySend(response);
}

// 出错处理
void IcsProxyTerminalClient::error() throw()
{
	// 已认证设备离线
	if (!_baseType::m_replaced && !m_connName.empty())
	{
		m_proxyServer.removeTerminalClient(m_connName);
		m_connName.clear();
	}
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

	/*
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
*/

	response.initHead(MessageId::C2T_auth_response, false);

	if (1)	// 成功
	{
		m_connName = std::move(gwId); // 保存检测点id

		response << ShortString("ok") << m_proxyServer.getHeartbeatTime();

		m_proxyServer.addTerminalClient(m_connName, shared_from_this());

		LOG_INFO("terminal " << m_connName << " created on " << this->name());

		this->setName(m_connName + "@" + this->name());
	}
	else
	{
		response << ShortString("failed");
	}
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
//		response.initHead(MessageId::MessageId_min);
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
	request.assertEmpty();

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


//---------------------------IcsProxyWebClient---------------------------//
IcsProxyWebClient::IcsProxyWebClient(IcsPorxyServer& localServer, socket&& s)
	: _baseType(std::move(s), "ProxyWeb")
	, m_proxyServer(localServer)
{

}

IcsProxyWebClient::~IcsProxyWebClient()
{

}

// 处理底层消息
void IcsProxyWebClient::handle(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{

}

// 处理平层消息
void IcsProxyWebClient::dispatch(ProtocolStream& request) throw(IcsException, otl_exception)
{

}


//---------------------------IcsForwardProxy---------------------------//
IcsForwardProxy::IcsForwardProxy(IcsPorxyServer& localServer, socket&& s)
	: _baseType(std::move(s), "ForwardProxy")
	, m_proxyServer(localServer)
{
}

IcsForwardProxy::~IcsForwardProxy()
{

}

// 处理底层消息
void IcsForwardProxy::handle(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	if (request.getHead()->getMsgID() == MessageId::T2T_auth_request)
	{
		handleAuthrize(request, response);
	}
	else
	{
		throw IcsException("unknow message id=0x%04x", (uint16_t)request.getHead()->getMsgID());
	}
}

// 处理平层消息
void IcsForwardProxy::dispatch(ProtocolStream& request) throw(IcsException, otl_exception)
{
	this->trySend(request);
}


void IcsForwardProxy::handleAuthrize(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	std::time_t t1, t3 = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	request >> t1;

	request.assertEmpty();

	// 解密该数据
	ics::decrypt(&t1, sizeof(t1));

	if (t3 - t1 < 3 * 60)	// 系统时间在3分钟内,认证成功
	{
		// 加密该数据
		ics::encrypt(&t1, sizeof(t1));
//		response.initHead(MessageId::T2T_auth_response);
		response << t1;

		m_proxyServer.addIcsCenterConn(shared_from_this());

		LOG_DEBUG("ics center authrize success, interval=" << t3 - t1);
	}
	else
	{
		throw IcsException("ics center authrize failed, interval=", t3 - t1);
	}

}

//---------------------------IcsPorxyServer---------------------------//
IcsPorxyServer::IcsPorxyServer(asio::io_service& ioService
	, const string& terminalAddr, std::size_t terminalMaxCount
	, const string& webAddr, std::size_t webMaxCount
	, const string& icsCenterAddr, std::size_t icsCenterCount)
	: m_ioService(ioService)
	, m_terminalTcpServer(ioService), m_terminalMaxCount(terminalMaxCount)
	, m_webTcpServer(ioService), m_webMaxCount(webMaxCount)
	, m_icsCenterTcpServer(ioService), m_icsCenterMaxCount(icsCenterCount)
{
	m_heartbeatTime = g_configFile.getAttributeInt("protocol", "heartbeat");


	m_terminalTcpServer.init("remote's terminal"
		, terminalAddr
		, [this](socket&& s)
	{
		std::shared_ptr<IcsConnection<icstcp>> conn(new IcsProxyTerminalClient(*this, std::move(s)));
		conn->start();
		m_timer.add(m_heartbeatTime, [conn, this](){
			return conn->timeout(m_heartbeatTime);
		});
	});

	m_webTcpServer.init("remote's web"
		, webAddr
		, [this](socket&& s)
	{
		std::shared_ptr<IcsConnection<icstcp>> conn(new IcsProxyWebClient(*this, std::move(s)));
		conn->start();
		m_timer.add(m_heartbeatTime, [conn, this]() {
			return conn->timeout(m_heartbeatTime);
		});
	});

	m_icsCenterTcpServer.init("remote's proxy"
		, icsCenterAddr
		, [this](socket&& s)
	{
		std::shared_ptr<IcsConnection<icstcp>> conn(new IcsForwardProxy(*this, std::move(s)));
		conn->start();
		m_timer.add(m_heartbeatTime, [conn, this](){
			return conn->timeout(m_heartbeatTime);
		});
	});

	m_timer.start();
}

IcsPorxyServer::~IcsPorxyServer()
{
	m_timer.stop();
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

/// 向终端发送数据
bool IcsPorxyServer::sendToTerminalClient(const string& conID, ProtocolStream& request)
{
	bool ret = false;
	std::lock_guard<std::mutex> lock(m_terminalConnMapLock);
	auto it = m_terminalConnMap.find(conID);
	if (it != m_terminalConnMap.end())
	{
		try {
			it->second->dispatch(request);
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
	m_icsCenterConnMap.push_back(conn);
}

/// 向全部ICS中心发送数据
void IcsPorxyServer::sendToIcsCenter(ProtocolStream& request)
{
	std::lock_guard<std::mutex> lock(m_icsCenterConnMapLock);
	for (auto& it : m_icsCenterConnMap)
	{
		ProtocolStream message(request);
		it->dispatch(message);
	}
}

}