
#ifndef _ICS_PROXY_SERVER_H
#define _ICS_PROXY_SERVER_H

#include "icsconnection.hpp"
#include "tcpserver.hpp"
#include <unordered_map>
#include <mutex>


namespace ics {

class IcsPorxyServer;

/// �ն�Э�鴦����
class IcsProxyTerminalClient : public IcsConnection<icstcp>
{
public:
	typedef IcsConnection<icstcp>	_baseType;
	typedef _baseType::socket		socket;
	typedef socket::shutdown_type	shutdown_type;

	IcsProxyTerminalClient(IcsPorxyServer& localServer, socket&& s);

	virtual ~IcsProxyTerminalClient();

	// ����ײ���Ϣ
	virtual void handle(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// ����ƽ����Ϣ
	virtual void dispatch(ProtocolStream& request) throw(IcsException, otl_exception);

private:
	// �ն���֤
	void handleAuthRequest(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// �¼��ϱ�
	void handleEventsReport(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// ҵ���ϱ�
	void handleBusinessReport(ProtocolStream& request, ProtocolStream& response)  throw(IcsException, otl_exception);
	
	// �ն˷�������������
	void handleHeartbeat(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);
	
	// �ն˷���ʱ��ͬ������
	void handleDatetimeSync(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

private:
	IcsPorxyServer&	m_proxyServer;

	std::string             m_connName;
	uint16_t				m_deviceKind;
	uint16_t		m_send_num;

	// business area
	uint32_t		m_lastBusSerialNum;

};

/// Զ��webЭ�鴦����
class IcsProxyWebClient : public IcsConnection<icstcp>
{
public:
	typedef IcsConnection<icstcp>	_baseType;
	typedef _baseType::socket		socket;
	typedef socket::shutdown_type	shutdown_type;

	IcsProxyWebClient(IcsPorxyServer& localServer, socket&& s);

	virtual ~IcsProxyWebClient();

	// ����ײ���Ϣ
	virtual void handle(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// ����ƽ����Ϣ
	virtual void dispatch(ProtocolStream& request) throw(IcsException, otl_exception);

private:
	// �ļ���������
	void handleTransFileRequest(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// �ļ�Ƭ�δ���
	void handleTransFileFrament(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

private:
	IcsPorxyServer&	m_proxyServer;
};

/// ת��ICS���Ĵ�����
class IcsProxyForward : public IcsConnection<icstcp>
{
public:
	typedef IcsConnection<icstcp>	_baseType;
	typedef _baseType::socket		socket;
	typedef socket::shutdown_type	shutdown_type;

	IcsProxyForward(IcsPorxyServer& localServer, socket&& s);

	virtual ~IcsProxyForward();

	// ����ײ���Ϣ
	virtual void handle(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// ����ƽ����Ϣ
	virtual void dispatch(ProtocolStream& request) throw(IcsException, otl_exception);

private:
	IcsPorxyServer&	m_proxyServer;
};



/// ���������
class IcsPorxyServer {
public:
	/// �����׽���
	typedef icstcp::socket socket;

	/// ����ָ��
	typedef std::unique_ptr<IcsConnection<icstcp>> ConneciontPrt;

	IcsPorxyServer(asio::io_service& ioService
		, const string& terminalAddr, std::size_t terminalMaxCount
		, const string& webAddr, std::size_t webMaxCount
		, const string& icsCenterAddr, std::size_t icsCenterCount);

	/// �������֤�ն�
	void addTerminalClient(const string& conID, ConneciontPrt conn);

	/// �Ƴ�����֤�ն�
	void removeTerminalClient(const string& conID);

	/// ���ն˷�������
	bool sendToTerminalClient(const string& conID, ProtocolStream& request);

	/// ���ICS���ķ�������
	void addIcsCenterConn(ConneciontPrt conn);

	/// ��ȫ��ICS���ķ�������
	void sendToIcsCenter(ProtocolStream& request);

private:
	asio::io_service& m_ioService;

	// �ն˷���
	TcpServer	m_terminalTcpServer;
	std::size_t m_terminalMaxCount;
	std::unordered_map<std::string, ConneciontPrt> m_terminalConnMap;
	std::mutex	m_terminalConnMapLock;
	uint16_t		m_heartbeatTime;


	// web����
	TcpServer	m_webTcpServer;
	std::size_t m_webMaxCount;

	// ics���ķ���
	TcpServer	m_icsCenterTcpServer;
	std::size_t m_icsCenterMaxCount;
	std::list<ConneciontPrt> m_icsCenterConnMap;
	std::mutex	m_icsCenterConnMapLock;


};

}

#endif // _ICS_PROXY_SERVER_H