

#ifndef _ICS_CONNECTION_H
#define _ICS_CONNECTION_H

#include "icsconfig.hpp"
#include "icsprotocol.hpp"
#include "mempool.hpp"
#include "log.hpp"
#include "database.hpp"
#include "downloadfile.hpp"
#include "clientmanager.hpp"
#include "icspushsystem.hpp"
#include <asio.hpp>
#include <asio/placeholders.hpp>
#include <functional>
#include <typeinfo>

namespace ics {
	template<class Protocol>
	class UnknownConnection;

	template<class Protocol>
	class IcsConnection;

	class PushSystem;

	template<class TerminalProtoType, class CenterProtoType>
	class ClientManager;
;
}

extern ics::MemoryPool g_memoryPool;
extern ics::DataBase g_database;
extern ics::IcsConfig g_configFile;
//extern ics::ClientManager<asio::ip::tcp, asio::ip::tcp> g_clientManager;
extern ics::PushSystem g_pushSystem;


namespace ics {

/// ICSЭ�鴦�����
template<class Protocol>
class IcsConnection {
public:

	typedef typename Protocol::socket socket;

	typedef typename socket::shutdown_type shutdown_type;

	IcsConnection(socket&& s)
		: m_socket(std::move(s))
		, m_request(g_memoryPool)
		, m_serialNum(0)
		, m_isSending(false)
		, m_replaced(false)
	{
	}

	virtual ~IcsConnection()
	{
		m_socket.close();
		LOG_INFO("connection " << m_connectionID << " leave");
	}

	void start(const uint8_t* data, std::size_t length)
	{
		m_request.reset();

		// �������Ϣ
		if (handleData(const_cast<uint8_t*>(data), length))
		{

		}
		// ��ʼ��������
		do_read();

	}

	void start()
	{
		m_request.reset();

		// ��ʼ��������
		do_read();
	}

	void replaced(bool flag = true)
	{
		m_replaced = flag;
	}

	// ����ײ���Ϣ
	virtual void handle(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception) = 0;

	// ����ƽ����Ϣ
	virtual void dispatch(ProtocolStream& request) throw(IcsException, otl_exception) = 0;

protected:
	void trySend(const MemoryChunk& mc)
	{
		{
			std::lock_guard<std::mutex> lock(m_sendLock);
			m_sendList.push_back(mc);
		}
		trySend();
	}

private:
	void do_read()
	{
		// wait response
		m_socket.async_receive(asio::buffer(m_recvBuff),
			[this](const std::error_code& ec, std::size_t length)
		{
			// no error and handle message
			if (!ec && this->handleData(m_recvBuff.data(), length))
			{
				// continue to read
				this->do_read();
			}
			else
			{
				this->do_error(shutdown_type::shutdown_both);
			}
		});
	}

	void do_write()
	{
		trySend();
	}

	void do_error(shutdown_type type)
	{
//		m_socket.shutdown(type);
		delete this;
	}

	void toHexInfo(const char* info, const uint8_t* data, std::size_t length)
	{
#ifndef NDEBUG
		char buff[1024];
		for (size_t i = 0; i < length && i < sizeof(buff) / 3; i++)
		{
			std::sprintf(buff + i * 3, " %02x", data[i]);
		}
		LOG_DEBUG(m_connectionID << " " << info << " " << length << " bytes...");
#endif
	}

	void trySend()
	{
		std::lock_guard<std::mutex> lock(m_sendLock);
		if (!m_isSending && !m_sendList.empty())
		{

			MemoryChunk& block = m_sendList.front();
			m_socket.async_send(asio::buffer(block.data, block.length),
				[this](const std::error_code& ec, std::size_t length)
			{
				if (!ec)
				{
					MemoryChunk& chunk = m_sendList.front();
					this->toHexInfo("send", chunk.data, chunk.length);
					m_sendList.pop_front();
					m_isSending = false;
					trySend();
				}
				else
				{
					LOG_DEBUG(m_connectionID << " send data error");
					do_error(shutdown_type::shutdown_both);
				}
			});

		}
	}


	bool handleData(uint8_t* data, std::size_t length)
	{
		/// show debug info
		this->toHexInfo("recv", data, length);

		try {
			while (length != 0)
			{
				/// assemble a complete ICS message and handle it
				if (m_request.assembleMessage(data, length))
				{
					handleMessage(m_request);
					m_request.reset();
				}
			}
		}
		catch (IcsException& ex)
		{
			LOG_ERROR(ex.message());
			return false;
		}

		return true;
	}

	void handleMessage(ProtocolStream&	request)
	{
		IcsMsgHead* head = request.getHead();

		try {
			/// verify ics protocol
			request.verify();

			/// ignore response message from terminal
			if (head->isResponse())
			{
				return;
			}


			ProtocolStream response(g_memoryPool);

			/// handle message
			handle(request, response);

			/// prepare response
			if (head->needResposne())
			{
				response.getHead()->init(MessageId::MessageId_min, head->getSendNum());	// head->getMsgID()
			}

			/// send response message
			if (!response.empty() || head->needResposne())
			{
				response.getHead()->setSendNum(m_serialNum++);
				response.serailzeToData();
				trySend(response.toMemoryChunk());
			}
		}
		catch (IcsException& ex)
		{
			throw IcsException("handle message [%04x] IcsException: %s", (uint32_t)head->getMsgID(), ex.message().c_str());
		}
		catch (otl_exception& ex)
		{
			throw IcsException("handle message [%04x] otl_exception: %s", (uint32_t)head->getMsgID(), ex.msg);
		}
		catch (std::exception& ex)
		{
			throw IcsException("handle message [%04x] std::exception: %s", (uint32_t)head->getMsgID(), ex.what());
		}
		catch (...)
		{
			throw IcsException("handle message [%04x] unknown error", (uint32_t)head->getMsgID());
		}
	}

protected:
	friend class UnknownConnection<Protocol>;
	// connect socket
	socket	m_socket;

	std::string m_connectionID;

	// recv area
	ProtocolStream m_request;
	std::array<uint8_t, 512> m_recvBuff;

	// send area
	uint16_t m_serialNum;
	std::list<MemoryChunk> m_sendList;
	std::mutex		m_sendLock;
	bool			m_isSending;

	bool			m_replaced;
};

/// �ն�Э�鴦����
template<class Protocol>
class IcsTerminal : public IcsConnection<Protocol>
{
public:
	typedef IcsConnection<Protocol>		_baseType;
	typedef typename _baseType::socket		socket;
	typedef typename socket::shutdown_type	shutdown_type;

	IcsTerminal(socket&& s)
		: _baseType(std::move(s))
	{
		m_onlineIP = g_configFile.getAttributeString("protocol", "onlineIP");
		m_onlinePort = g_configFile.getAttributeInt("protocol", "onlinePort");
		m_heartbeatTime = g_configFile.getAttributeInt("protocol", "heartbeat");
	}

	virtual ~IcsTerminal()
	{
		if (!_baseType::m_replaced && !_baseType::m_connectionID.empty())
		{
			s_terminalConnMap.erase(_baseType::m_connectionID);

			OtlConnectionGuard connGuard(g_database);

			otl_stream s(1
				, "{ call sp_offline(:id<char[33],in>,:ip<char[17],in>,:port<int,in>) }"
				, connGuard.connection());

			s << _baseType::m_connectionID << m_onlineIP << m_onlinePort;
		}
	}

	// ����ײ���Ϣ
	virtual void handle(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
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
			throw IcsException("unknown terminal message id: %d ", request.getHead()->getMsgID());
			break;
		}

#if defined(ICS_CENTER_MODE)
#else
		request.rewind();
		ProtocolStream forwardStream(g_memoryPool);
		forwardStream << m_conn_name << request;
		conn.forwardToCenter(forwardStream);
#endif

	}

	// ����ƽ����Ϣ
	virtual void dispatch(ProtocolStream& request) throw(IcsException, otl_exception)
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
		IcsMsgHead* msgHead = response.getHead();
		msgHead->init((MessageId)messageID, false);
		msgHead->setSendNum(m_send_num++);
		response << request;

		_baseType::trySend(response.toMemoryChunk());
	}

private:
#define UPGRADE_FILE_SEGMENG_SIZE 1024

	// �ն���֤
	void handleAuthRequest(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
	{
		if (!m_conn_name.empty())
		{
			LOG_DEBUG(m_conn_name << " ignore repeat authrize message");
			response.getHead()->init(MessageId::C2T_auth_response, request.getHead()->getSendNum());
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

		response.getHead()->init(MessageId::C2T_auth_response, request.getHead()->getSendNum());

		if (ret == 0)	// �ɹ�
		{
			_baseType::m_connectionID = std::move(gwId); // �������id
			otl_stream onlineStream(1
				, "{ call sp_online(:id<char[33],in>,:ip<char[16],in>,:port<int,in>) }"
				, connGuard.connection());

			onlineStream << _baseType::m_connectionID << m_onlineIP << m_onlinePort;

			response << ShortString("ok") << m_heartbeatTime;

			auto& oldConn = s_terminalConnMap[_baseType::m_connectionID];
			if (!oldConn)
			{
				oldConn->replaced(true);
				delete oldConn;			
			}
			oldConn = this;
//			g_clientManager.addTerminalConn(_baseType::m_connectionID, this);

			LOG_INFO("connection " << _baseType::m_connectionID << " arrive");
		}
		else
		{
			response << ShortString("failed");
		}

	}

	// ��׼״̬�ϱ�
	void handleStdStatusReport(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception, otl_exception)
	{
		uint32_t status_type;	// ��׼״̬���

		request >> status_type;

		// ͨ�ú���
		if (status_type == 1)
		{
			uint8_t device_ligtht;	// �豸ָʾ��
			LongString device_status;	// �豸״̬
			uint8_t cheat_ligtht;	// ����ָʾ��
			string cheat_status;	// ����״̬
			float zero_point;		// �������	

			request >> device_ligtht >> device_status >> cheat_ligtht >> cheat_status >> zero_point;

			request.assertEmpty();

			IcsDataTime recv_time;
			ics::getIcsNowTime(recv_time);

			OtlConnectionGuard connGuard(g_database);

			otl_stream o(1,
				"{ call sp_status_standard(:id<char[33],in>,:devFlag<int,in>,:devStat<char[512],in>,:cheatFlag<int,in>,:cheatStat<char[512],in>,:zeroPoint<float,in>,:recvTime<timestamp,in>) }"
				, connGuard.connection());

			o << m_conn_name << (int)device_ligtht << device_status << (int)cheat_ligtht << cheat_status << zero_point << recv_time;
		}
		// ����
		else
		{
			LOG_WARN("unknown standard status type:" << status_type);
		}
		//*/
	}

	// �Զ���״̬�ϱ�
	void handleDefStatusReport(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
	{
		/*


		MessageBuffer request(msghead + 1, msghead->length - sizeof(IcsMsgHead)-sizeof(uint32_t));	// ��Ϣ������

		CDatetime status_time;		//	״̬�ɼ�ʱ��
		uint16_t status_count;		//	״̬�����


		uint16_t status_id = 0;		//	״̬���
		uint8_t status_light = 0;	//	״ָ̬ʾ��
		uint8_t status_type = 0;	//	״ֵ̬����
		string status_value;		//	״ֵ̬

		request >> status_time >> status_count;

		for (uint16_t i = 0; i<status_count; i++)
		{
		request >> status_id >> status_light >> status_type >> status_value;

		// �������ݿ�
		LOG4CPLUS_DEBUG("light:" << (int)status_light << ",id:" << status_id << ",type:" << (int)status_type << ",value:" << status_value);

		// store into database
		std::vector<Variant> params;
		params.push_back(terminal->m_monitorID);// ������
		params.push_back(int(status_id));		// ״̬���
		params.push_back(int(status_light));	// ״ָ̬ʾ��
		params.push_back(int(status_type));		// ״ֵ̬����
		params.push_back(status_value);			//״ֵ̬
		params.push_back(status_time);			//״̬�ɼ�ʱ��

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

	// �¼��ϱ�
	void handleEventsReport(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
	{
		IcsDataTime event_time, recv_time;	//	�¼�����ʱ�� ����ʱ��
		uint16_t event_count;	// �¼������

		uint16_t event_id = 0;	//	�¼����
		uint8_t event_type = 0;	//	�¼�ֵ����
		string event_value;		//	�¼�ֵ

		getIcsNowTime(recv_time);

		request >> event_time >> event_count;


		OtlConnectionGuard connGuard(g_database);

		otl_stream eventStream(1
			, "{ call sp_event_report(:id<char[33],in>,:devKind<int,in>,:eventID<int,in>,:eventType<int,in>,:eventValue<char[256],in>,:eventTime<timestamp,in>,:recvTime<timestamp,in>) }"
			, connGuard.connection());


		// ����ȡ��ȫ���¼�
		for (uint16_t i = 0; i<event_count; i++)
		{
			request >> event_id >> event_type >> event_value;

			eventStream << m_conn_name << (int)m_deviceKind << (int)event_id << (int)event_type << event_value << event_time << recv_time;
			
			// ���͸����ͷ�����: ����ID ����ʱ�� �¼���� �¼�ֵ
			ProtocolStream pushStream(g_memoryPool);			
			pushStream << _baseType::m_connectionID << m_deviceKind << event_time << event_id << event_value;
			
			g_pushSystem.send(pushStream);
		}
		request.assertEmpty();
	}

	// ҵ���ϱ�
	void handleBusinessReport(ProtocolStream& request, ProtocolStream& response)  throw(IcsException, otl_exception)
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

			s << m_conn_name << (int)business_no << cargo_num << vehicle_num
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

				s << m_conn_name << business_no << (int)amount << total_weight << single_weighet << report_time << recv_time;
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

			s << m_conn_name << (int)business_no << (int)total_weight << speed*0.1 << (int)axle_num << axle_str << (int)type_num << type_str << report_time << recv_time;
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

	// GPS�ϱ�
	void handleGpsReport(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
	{
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

		request.assertEmpty();

		OtlConnectionGuard connGuard(g_database);

		otl_stream s(1
			, "{ call `ics_canchu`.sp_gps_report(:id<char[33],in>,:logFlag<int,in>,:logitude<int,in>,:laFlag<int,in>,:latitude<int,in>,:signal<int,in>,:height<int,in>,:speed<int,in>) }"
			, connGuard.connection());

		s << m_conn_name << (int)postionFlag.longitude_flag << (int)longitude << (int)postionFlag.latitude_flag << (int)latitude << (int)postionFlag.signal << (int)height << (int)speed;
	}

	// �ն˻�Ӧ������ѯ
	void handleParamQueryResponse(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
	{
		/*


		MessageBuffer request(msghead + 1, msghead->length - sizeof(IcsMsgHead)-sizeof(uint32_t));	// ��Ϣ������

		uint32_t request_id;	// ����id
		uint16_t param_count;	// ��������

		uint8_t net_id = 0;		//	�������
		uint16_t param_id = 0;	//	�������
		uint8_t param_type = 0;	//	����ֵ����
		string param_value;		//	����ֵ

		request >> request_id >> param_count;

		for (uint16_t i = 0; i<param_count; i++)
		{
		request >> net_id >> param_id >> param_type >> param_value;
		// �������ݿ�

		}

		if (!request.empty())
		{
		LOG_WARN("reply query paramter message has superfluous data:" << request.leftSize());
		return false;
		}

		//*/
	}

	// �ն������ϱ������޸�
	void handleParamAlertReport(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
	{
		/*


		MessageBuffer request(msghead + 1, msghead->length - sizeof(IcsMsgHead)-sizeof(uint32_t));	// ��Ϣ������

		CDatetime alert_time;	//	�޸�ʱ��
		uint16_t param_count;	// ��������

		uint8_t net_id = 0;		//	�������
		uint16_t param_id = 0;	//	�������
		uint8_t param_type = 0;	//	����ֵ����
		string param_value;		//	����ֵ

		request >> alert_time >> param_count;

		for (uint16_t i = 0; i<param_count; i++)
		{
		request >> net_id >> param_id >> param_type >> param_value;

		// �������ݿ�

		}

		if (!request.empty())
		{
		LOG_WARN("report alert paramter message has superfluous data:" << request.leftSize());
		return false;
		}
		//*/
	}

	// �ն˻�Ӧ�����޸�
	void handleParamModifyResponse(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
	{
		/*


		MessageBuffer request(msghead + 1, msghead->length - sizeof(IcsMsgHead)-sizeof(uint32_t));	// ��Ϣ������

		uint32_t request_id;	// ����id
		uint16_t param_count;	// ��������

		uint8_t net_id = 0;		//	�������
		uint16_t param_id = 0;	//	�������
		string alert_value;		//	�޸Ľ��

		request >> request_id >> param_count;

		for (uint16_t i = 0; i<param_count; i++)
		{
		request >> net_id >> param_id >> alert_value;
		// �������ݿ�

		}

		if (!request.empty())
		{
		LOG_WARN("reply modify paramter message has superfluous data:" << request.leftSize());
		return false;
		}
		//*/
	}

	// �ն˷���ʱ��ͬ������
	void handleDatetimeSync(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
	{
		IcsDataTime dt1, dt2;
		request >> dt1;
		request.assertEmpty();

		getIcsNowTime(dt2);

		response.getHead()->init(MessageId::MessageId_min, m_send_num++);
		response << dt1 << dt2 << dt2;
	}

	// �ն��ϱ���־
	void handleLogReport(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
	{
		IcsDataTime status_time;		//	ʱ��
		uint8_t log_level;			//	��־����
		uint8_t encode_type = 0;	//	���뷽ʽ
		string log_value;			//	��־����

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

		s << m_conn_name << status_time << (int)log_level << log_value;
	}

	// �ն˷�������������
	void handleHeartbeat(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
	{

	}

	// �ն˾ܾ���������
	void handleDenyUpgrade(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
	{
		uint32_t request_id;	// ����id
		string reason;	// �ܾ�����ԭ��

		request >> request_id >> reason;

		request.assertEmpty();

		OtlConnectionGuard connGuard(g_database);
		otl_stream s(1
			, "{ call sp_upgrade_refuse(:id<char[33],in>,:reqID<int,in>,:reason<char[126],in>) }"
			, connGuard.connection());

		s << m_conn_name << int(request_id) << reason;
	}

	// �ն˽�����������
	void handleAgreeUpgrade(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
	{
		uint32_t request_id;	// �ļ�id
		request >> request_id;
		request.assertEmpty();

		OtlConnectionGuard connGuard(g_database);
		otl_stream s(1
			, "{ call sp_upgrade_accept(:id<char[33],in>,:reqID<int,in>) }"
			, connGuard.connection());

		s << m_conn_name << int(request_id);
	}

	// ��Ҫ�����ļ�Ƭ��
	void handleRequestFile(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
	{

		uint32_t file_id, request_id, fragment_offset, received_size;

		uint16_t fragment_length;

		request >> file_id >> request_id >> fragment_offset >> fragment_length >> received_size;

		request.assertEmpty();

		// ������������(��ѯ������id��Ӧ��״̬)
		OtlConnectionGuard connGuard(g_database);
		otl_stream s(1
			, "{ call sp_upgrade_set_progress(:F1<int,in>,:F2<int,in>,@stat) }"
			, connGuard.connection());

		s << m_conn_name << (int)request_id << (int)received_size;

		otl_stream queryResutl(1, "select @stat :#<int>", connGuard.connection());

		int result = 0;

		queryResutl >> result;

		// �����ļ�
		auto fileInfo = FileUpgradeManager::getInstance()->getFileInfo(file_id);


		// �鿴�Ƿ���ȡ������
		if (result == 0 && fileInfo)	// ����״̬���ҵ����ļ�
		{
			if (fragment_offset > fileInfo->file_length)	// �����ļ���С
			{
				throw IcsException("require file offset [%d] is more than file's length [%d]", fragment_offset, fileInfo->file_length);
			}
			else if (fragment_offset + fragment_length > fileInfo->file_length)	// �����ļ���С��ȡʣ���С
			{
				fragment_length = fileInfo->file_length - fragment_offset;
			}

			if (fragment_length > UPGRADE_FILE_SEGMENG_SIZE)	// �����ļ�Ƭ�����Ϊ������ʣ�೤��
			{
				fragment_length = UPGRADE_FILE_SEGMENG_SIZE;
			}

			response << file_id << request_id << fragment_offset << fragment_length;

			response.append((char*)fileInfo->file_content + fragment_offset, fragment_length);

		}
		else	// ����������
		{
			response.getHead()->init(MessageId::T2C_upgrade_not_found, request.getHead()->getAckNum());
		}
	}

	// �����ļ�������
	void handleUpgradeResult(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
	{
		uint32_t request_id;	// �ļ�id
		string upgrade_result;	// �������

		request >> request_id >> upgrade_result;

		request.assertEmpty();

		OtlConnectionGuard connGuard(g_database);
		otl_stream s(1
			, "{ call sp_upgrade_result(:F1<int,in>,:F3<char[126],in>) }"
			, connGuard.connection());

		s << (int)request_id << upgrade_result;
	}

	// �ն�ȷ��ȡ������
	void handleUpgradeCancelAck(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
	{
		uint32_t request_id;	// �ļ�id

		request >> request_id;

		request.assertEmpty();

		OtlConnectionGuard connGuard(g_database);
		otl_stream s(1
			, "{ call sp_upgrade_cancel_ack(:F1<int,in>) }"
			, connGuard.connection());

		s << (int)request_id;
	}

private:
	std::string             m_conn_name;
	uint16_t				m_deviceKind;

	// recv area
	std::array<uint8_t, 512> m_recvBuff;
	uint16_t		m_send_num;
	uint8_t			m_recv_buf[512];

	// send area
	std::list<MemoryChunk> m_sendList;
	std::mutex		m_sendLock;
	bool			m_isSending;

	// business area
	uint32_t		m_lastBusSerialNum;

	// ip and port
	std::string		m_onlineIP;
	int				m_onlinePort;
	uint16_t		m_heartbeatTime;

public:
	// global connection
	static std::unordered_map<std::string, _baseType*> s_terminalConnMap;
};

template<class Protocol>
std::unordered_map<std::string, IcsConnection<Protocol>*> IcsTerminal<Protocol>::s_terminalConnMap;

/// webЭ�鴦����
template<class Protocol>
class IcsWeb : public IcsConnection<Protocol>
{
public:
	typedef IcsConnection<Protocol>		_baseType;
	typedef typename _baseType::socket		socket;
	typedef typename socket::shutdown_type	shutdown_type;

	IcsWeb(socket&& s)
		: _baseType(std::move(s))
	{

	}

	// ����ײ���Ϣ
	virtual void handle(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
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

	// ����ƽ����Ϣ
	virtual void dispatch(ProtocolStream& request) throw(IcsException, otl_exception)
	{
		throw IcsException("WebHandler never handle message, id: %d ", request.getHead()->getMsgID());
	}

private:

	// ת������Ӧ�ն�
	void handleForward(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
	{
		ShortString terminalName;
		uint16_t messageID;
		uint32_t requestID;
		request >> terminalName >> messageID >> requestID;
		request.rewind();

		// �����ն�IDת������Ϣ
		bool result = false;
		auto conn = IcsTerminal<Protocol>::s_terminalConnMap.find(terminalName);
		if (conn != IcsTerminal<Protocol>::s_terminalConnMap.end())
		{
			conn->second->dispatch(request);
			result = true;
		}

		// ת�������¼�����ݿ�
		{
			OtlConnectionGuard connection(g_database);
			otl_stream s(1, "{ call sp_web_command_status(:requestID<int,in>,:msgID<int,in>,:stat<int,in>) }", connection.connection());
			s << (int)requestID << (int)messageID << (result ? 0 : 1);
		}
	}

	// ����Զ���ӷ�����
	void handleConnectRemote(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
	{
		ShortString remoteID;
		request >> remoteID;
		request.assertEmpty();

		// �����ݿ��в�ѯ��Զ�˵�ַ
		OtlConnectionGuard connGuard(g_database);

		otl_stream queryStream(1
			, "{ call sp_query_remote_server(:id<char[33],in>,@ip,@port) }"
			, connGuard.connection());

		queryStream << remoteID;

		otl_stream getStream(1, "select @ip :#<char[32]>,@port :#<int>", connGuard.connection());

		string remoteIP;
		int remotePort;

		getStream >> remoteIP >> remotePort;

		// ���Ӹ�Զ��
		bool connectResult = true;
		asio::ip::tcp::socket remoteSocket(_baseType::m_socket);
		asio::ip::tcp::endpoint endpoint(asio::ip::address::from_string(remoteIP), remotePort);
		asio::error_code ec;
		remoteSocket.connect(endpoint, ec);
		

		// ��Ӧweb���ӽ��:0-�ɹ�,1-ʧ��
		response << remoteID << (uint8_t)(!ec ? 0 : 1);

		if (!ec)
		{
			IcsConnection<asio::ip::tcp>* ic = new ProxyServer<asio::ip::tcp>(std::move(remoteSocket));
			ic->start();
			s_remoteServerConnMap<asio::ip::tcp>[remoteID] = ic;
		}
	}

	// �Ͽ�Զ���ӷ�����
	void handleDisconnectRemote(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
	{
		ShortString remoteID;
		request >> remoteID;
		request.assertEmpty();

		// �����ݿ��в�ѯ��Զ�˵�ַ
		OtlConnectionGuard connGuard(g_database);

		otl_stream queryStream(1
			, "{ call sp_query_remote_server(:id<char[33],in>,@ip,@port) }"
			, connGuard.connection());

		queryStream << remoteID;

		otl_stream getStream(1, "select @ip :#<char[32]>,@port :#<int>", connGuard.connection());

		string remoteIP;
		int remotePort;

		getStream >> remoteIP >> remotePort;

		// �Ͽ���Զ��
		bool connectResult = true;

		// ��Ӧweb���ӽ��:0-�ɹ�,1-ʧ��
		response << remoteID << (uint8_t)(connectResult ? 0 : 1);
	}

	// �ļ���������
	void handleTransFileRequest(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
	{

	}

	// �ļ�Ƭ�δ���
	void handleTransFileFrament(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
	{

	}

private:
	// global connection
	static std::unordered_map<std::string, _baseType*> s_remoteServerConnMap;
};

template<class Protocol>
std::unordered_map<std::string, IcsConnection<Protocol>*> IcsWeb<Protocol>::s_remoteServerConnMap;

/// �ӷ����������Э�鴦����
template<class Protocol>
class ProxyServer : public IcsConnection<Protocol>
{
public:
	typedef IcsConnection<Protocol>		_baseType;
	typedef typename _baseType::socket		socket;
	typedef typename socket::shutdown_type	shutdown_type;

	ProxyServer(socket&& s)
		: _baseType(std::move(s))
	{

	}

	// ����ײ���Ϣ
	virtual void handle(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
	{
		LOG_DEBUG("IcsWeb handle");
	}

	// ����ƽ����Ϣ
	virtual void dispatch(ProtocolStream& request) throw(IcsException, otl_exception)
	{
		LOG_DEBUG("IcsWeb dispatch");
	}
};

/// �ӷ������ͻ���Э�鴦����
template<class Protocol>
class ProxyClient : public IcsConnection<Protocol>
{
public:
	typedef IcsConnection<Protocol>		_baseType;
	typedef typename _baseType::socket		socket;
	typedef typename socket::shutdown_type	shutdown_type;

	ProxyClient(socket&& s)
		: _baseType(std::move(s))
	{

	}

	// ����ײ���Ϣ
	virtual void handle(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
	{
		LOG_DEBUG("IcsWeb handle");
	}

	// ����ƽ����Ϣ
	virtual void dispatch(ProtocolStream& request) throw(IcsException, otl_exception)
	{
		LOG_DEBUG("IcsWeb dispatch");
	}
};


template<class Protocol>
class PushClient : public IcsConnection<Protocol>
{
public:
	typedef IcsConnection<Protocol>		_baseType;
	typedef typename _baseType::socket		socket;
	typedef typename socket::shutdown_type	shutdown_type;

	PushClient(socket&& s)
		: _baseType(std::move(s))
	{

	}

	// ����ײ���Ϣ
	virtual void handle(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
	{
		throw IcsException("PushClient never handle")
	}

	// ����ƽ����Ϣ
	virtual void dispatch(ProtocolStream& request) throw(IcsException, otl_exception)
	{
		_baseType::trySend(request.toMemoryChunk());
	}
};


// origin connection
template<class Protocol>
class UnknownConnection {
public:
	typedef typename Protocol::socket socket;

	UnknownConnection(socket&& s)
		: m_socket(std::move(s))
	{

	}

	void start()
	{
		m_socket.async_receive(asio::buffer(m_recvBuff)
			, [this](const asio::error_code& ec, std::size_t len)
		{
			readHanler(ec, len);
		});
	}

private:

	void readHanler(const asio::error_code& ec, std::size_t len)
	{
		if (!ec && len > sizeof(IcsMsgHead)+IcsMsgHead::CrcCodeSize)
		{
			auto msgid = ((IcsMsgHead*)m_recvBuff.data())->getMsgID();
			IcsConnection<Protocol>* ic;
			if (msgid > MessageId::T2C_min && msgid < MessageId::T2C_max)
			{
				ic = new IcsTerminal<Protocol>(std::move(m_socket));
			}
			else if (msgid > MessageId::W2C_min && msgid < MessageId::W2C_max)
			{
				ic = new IcsWeb<Protocol>(std::move(m_socket));
			}
			else if (msgid == MessageId::T2T_forward_msg)
			{
//				IcsConnection<Protocol>* ic = new IcsTerminal<Protocol>(std::move(m_socket));
			}
			else
			{
				LOG_WARN("unknow message id = " << msgid);
			}

			if (ic)
			{
				ic->start(m_recvBuff.data(), len);
			}
		}
		else
		{
			LOG_WARN("need more data, length = " << len);
		}

		// ���ۺ��ֽ��ɾ������
		delete this;
	}

	socket	m_socket;
	std::array<uint8_t, 64> m_recvBuff;
};



}
#endif // _ICS_CONNECTION_H
