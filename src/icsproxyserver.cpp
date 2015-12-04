
#include "icsproxyserver.hpp"

extern ics::IcsConfig g_configFile;


namespace ics {

//---------------------------IcsProxyTerminalClient---------------------------//
IcsProxyTerminalClient::IcsProxyTerminalClient(IcsPorxyServer& localServer, socket&& s)
	: _baseType(std::move(s))
	, m_proxyServer(localServer)
{
}

IcsProxyTerminalClient::~IcsProxyTerminalClient()
{
	// ����֤�豸����
	if (!_baseType::m_replaced && !m_connName.empty())
	{

	}

}

// ����ײ���Ϣ
void IcsProxyTerminalClient::handle(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	switch (request.getHead()->getMsgID())
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
		LOG_WARN(_baseType::m_name << " recv unknown terminal message id = " << request.getHead()->getMsgID());
//		throw IcsException("%s recv unknown terminal message id = %04x ",_baseType::m_name.c_str(), request.getHead()->getMsgID());
		break;
	}
}

// ����ƽ����Ϣ
void IcsProxyTerminalClient::dispatch(ProtocolStream& request) throw(IcsException, otl_exception)
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

	// ��¼������IDת�����


	// ���͵������ӶԶ�
	ProtocolStream response(g_memoryPool);
	response.getHead()->init((MessageId)messageID, false);
	response << request;

	_baseType::trySend(response);
}


// �ն���֤
void IcsProxyTerminalClient::handleAuthRequest(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	if (!m_connName.empty())
	{
		LOG_DEBUG(m_connName << " ignore repeat authrize message");
		response.getHead()->init(MessageId::C2T_auth_response, request.getHead()->getSendNum());
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

	response.getHead()->init(MessageId::C2T_auth_response, request.getHead()->getSendNum());

	if (ret == 0)	// �ɹ�
	{
		m_connName = std::move(gwId); // �������id
		otl_stream onlineStream(1
			, "{ call sp_online(:id<char[33],in>,:ip<char[16],in>,:port<int,in>) }"
			, connGuard.connection());

		onlineStream << m_connName << m_proxyServer.m_onlineIP << m_proxyServer.m_onlinePort;

		response << ShortString("ok") << m_proxyServer.m_heartbeatTime;

		m_proxyServer.addTerminalClient(m_connName, std::unique_ptr<IcsConnection<icstcp>>(this));

		LOG_INFO("terminal " << m_connName << " created on " << _baseType::name());
	}
	else
	{
		response << ShortString("failed");
	}
	*/
}

// �¼��ϱ�
void IcsProxyTerminalClient::handleEventsReport(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	IcsDataTime event_time, recv_time;	//	�¼�����ʱ�� ����ʱ��
	uint16_t event_count;	// �¼������

	uint16_t event_id = 0;	//	�¼����
	uint8_t event_type = 0;	//	�¼�ֵ����
	string event_value;		//	�¼�ֵ

	getIcsNowTime(recv_time);

	request >> event_time >> event_count;

	/*
	OtlConnectionGuard connGuard(g_database);

	otl_stream eventStream(1
		, "{ call sp_event_report(:id<char[33],in>,:devKind<int,in>,:eventID<int,in>,:eventType<int,in>,:eventValue<char[256],in>,:eventTime<timestamp,in>,:recvTime<timestamp,in>) }"
		, connGuard.connection());


	// ����ȡ��ȫ���¼�
	for (uint16_t i = 0; i < event_count; i++)
	{
		request >> event_id >> event_type >> event_value;

		eventStream << m_connName << (int)m_deviceKind << (int)event_id << (int)event_type << event_value << event_time << recv_time;

		// ���͸����ͷ�����: ����ID ����ʱ�� �¼���� �¼�ֵ
		ProtocolStream pushStream(g_memoryPool);
		pushStream << m_connName << m_deviceKind << event_time << event_id << event_value;

		m_proxyServer.m_pushSystem.send(pushStream);
	}
	*/

	request.assertEmpty();
}

// ҵ���ϱ�
void IcsProxyTerminalClient::handleBusinessReport(ProtocolStream& request, ProtocolStream& response)  throw(IcsException, otl_exception)
{
	IcsDataTime report_time, recv_time;			// ҵ��ɼ�ʱ�� ����ʱ��
	uint32_t business_no;	// ҵ����ˮ��
	uint32_t business_type;	// ҵ������

	getIcsNowTime(recv_time);

	request >> report_time >> business_no >> business_type;

	if (m_lastBusSerialNum == business_no)	// �ظ���ҵ����ˮ�ţ�ֱ�Ӻ���
	{
		response.getHead()->init(MessageId::MessageId_min);
		return;
	}

	/*
	OtlConnectionGuard connGuard(g_database);

	m_lastBusSerialNum = business_no;	// ���������ҵ����ˮ��

	if (business_type == 1)	// ��̬������
	{
		ShortString cargo_num;	// ���ﵥ��	
		ShortString vehicle_num;	// ����
		ShortString consigness;	// �ջ���λ
		ShortString cargo_name;	// ��������

		float weight1, weight2, weight3, weight4, unit_price, money;	// ë�� Ƥ�� ���� ���� ���� ���
		uint8_t in_out;	// ����

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
	else if (business_type == 2)	// ��װ��
	{
		uint8_t count;	// ������
		request >> count;

		otl_stream s(1
			, "{ call `ics_packing`.sp_business_pack(:id<char[33],in>,:num<int,in>,:amount<int,in>,:weight<int,in>,:sWeight<int,in>,:F6<float,in>,:reportTime<timestamp,in>,:recvTime<timestamp,in>) }"
			, connGuard.connection());

		for (uint8_t i = 0; i < count; i++)	// ����ȡ���ӵĳ�������
		{
			uint8_t number;		// �ӱ��
			uint16_t amount;	// ���ش���
			float total_weight;		// ��������
			float single_weighet;// ��������

			request >> number >> amount >> total_weight >> single_weighet;

			s << m_connName << business_no << (int)amount << total_weight << single_weighet << report_time << recv_time;
		}
	}
	else if (business_type == 3)	// ��·����
	{
		char buff[126];
		string axle_str;	// �����ַ���������1,����2,����3 ......
		string type_str;	// �������ַ���������1,����2,����3 ......

		uint32_t total_weight;// ����
		uint16_t speed;	// ����
		uint8_t axle_num;	// ����(1-20)
		uint8_t type_num;	// ��������(1-����)

		request >> total_weight >> speed >> axle_num;

		// �����������
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

		// �������������
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
	else if (business_type == 4)	// �ͳ���
	{
		union
		{
			uint8_t	data;
			struct {
				uint8_t mode : 1;	// ����ģʽ��0-�ֶ�ģʽ��1-�Զ�ģʽ
				uint8_t unit : 1;	// ������λ��0-ǧ�ˣ�1-��
				uint8_t card : 1;	// ˢ��״̬��0-�û�ˢ����1-˾��ˢ��
				uint8_t flow : 1;	// ����״̬��0-����������1-�����쳣
				uint8_t evalution : 2;	// ���ۣ�0-������1-������2-����
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
				uint8_t longitude_flag : 1;	// ���ȷ��ţ�0-������1-����
				uint8_t latitude_flag : 1;	// γ�ȷ��ţ�0-��γ��1-��γ
				uint8_t signal : 3;	// GPS�ź�ǿ�ȣ�0-��ǿ��1-��ǿ��2-һ�㣬3-������4-��
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

// �ն˷�������������
void IcsProxyTerminalClient::handleHeartbeat(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{

}

// �ն˷���ʱ��ͬ������
void IcsProxyTerminalClient::handleDatetimeSync(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	IcsDataTime dt1, dt2;
	request >> dt1;
	request.assertEmpty();

	getIcsNowTime(dt2);

	response.getHead()->init(MessageId::MessageId_min, m_send_num++);
	response << dt1 << dt2 << dt2;
}


//---------------------------IcsProxyWebClient---------------------------//
IcsProxyWebClient::IcsProxyWebClient(IcsPorxyServer& localServer, socket&& s)
	: _baseType(std::move(s))
	, m_proxyServer(localServer)
{
	_baseType::m_name = "ProxyWeb@" + _baseType::m_name;
}

IcsProxyWebClient::~IcsProxyWebClient()
{

}

// ����ײ���Ϣ
void IcsProxyWebClient::handle(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{

}

// ����ƽ����Ϣ
void IcsProxyWebClient::dispatch(ProtocolStream& request) throw(IcsException, otl_exception)
{

}


//---------------------------IcsProxyForward---------------------------//
IcsProxyForward::IcsProxyForward(IcsPorxyServer& localServer, socket&& s)
	: _baseType(std::move(s))
	, m_proxyServer(localServer)
{
	_baseType::m_name = "ProxyForward@" + _baseType::m_name;
}

IcsProxyForward::~IcsProxyForward()
{

}

// ����ײ���Ϣ
void IcsProxyForward::handle(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{

}

// ����ƽ����Ϣ
void IcsProxyForward::dispatch(ProtocolStream& request) throw(IcsException, otl_exception)
{

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


	m_terminalTcpServer.init(terminalAddr
		, [this](socket&& s)
	{
		auto conn = new IcsProxyTerminalClient(*this, std::move(s));
		conn->start();
	});

	m_webTcpServer.init(webAddr
		, [this](socket&& s)
	{
		auto conn = new IcsProxyWebClient(*this, std::move(s));
		conn->start();
	});

	m_icsCenterTcpServer.init(icsCenterAddr
		, [this](socket&& s)
	{
		auto conn = new IcsProxyForward(*this, std::move(s));
		conn->start();
	});
}

/// �������֤�ն�
void IcsPorxyServer::addTerminalClient(const string& conID, ConneciontPrt conn)
{
	std::lock_guard<std::mutex> lock(m_terminalConnMapLock);
	auto& c = m_terminalConnMap[conID];
	if (c)
	{
		c->replaced();
	}
	c = std::move(conn);
}

/// �Ƴ�����֤�ն�
void IcsPorxyServer::removeTerminalClient(const string& conID)
{
	std::lock_guard<std::mutex> lock(m_terminalConnMapLock);
	m_terminalConnMap.erase(conID);
}

/// ���ն˷�������
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

/// ���ICS���ķ�������
void IcsPorxyServer::addIcsCenterConn(ConneciontPrt conn)
{
	std::lock_guard<std::mutex> lock(m_icsCenterConnMapLock);
	m_icsCenterConnMap.push_back(std::move(conn));
}

/// ��ȫ��ICS���ķ�������
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