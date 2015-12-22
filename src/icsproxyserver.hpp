
#ifndef _ICS_PROXY_SERVER_H
#define _ICS_PROXY_SERVER_H

#include "icsconnection.hpp"
#include "tcpserver.hpp"
#include "timer.hpp"
#include <unordered_map>
#include <mutex>
#include <set>


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

	// ������
	virtual void error() throw();

private:
	/// ת����ICS����
	void forwardToIcsCenter(ProtocolStream& request);

	/// ������֪ͨ:status 0-online,1-offline
	void onoffLineToIcsCenter(uint8_t status);

	// �ն���֤
	void handleAuthRequest(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// ��׼״̬�ϱ�
	void handleStdStatusReport(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception, otl_exception);

	// �Զ���״̬�ϱ�
	void handleDefStatusReport(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// �¼��ϱ�
	void handleEventsReport(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// ҵ���ϱ�
	void handleBusinessReport(ProtocolStream& request, ProtocolStream& response)  throw(IcsException, otl_exception);
	
	// �ն˷�������������
	void handleHeartbeat(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);
	
	// �ն˷���ʱ��ͬ������
	void handleDatetimeSync(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// GPS�ϱ�
	void handleGpsReport(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);
	
	// �ն��ϱ���־
	void handleLogReport(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// �ն˾ܾ���������
	void handleDenyUpgrade(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// �ն˽�����������
	void handleAgreeUpgrade(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// ��Ҫ�����ļ�Ƭ��
	void handleRequestFile(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// �����ļ�������
	void handleUpgradeResult(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// �ն�ȷ��ȡ������
	void handleUpgradeCancelAck(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

private:
	IcsPorxyServer&	m_proxyServer;

	std::string     m_connName;
	uint16_t		m_deviceKind;
	uint16_t		m_send_num;

	// business area
	uint32_t		m_lastBusSerialNum;

};



/// ICS���Ĵ�����
class IcsCenter : public IcsConnection<icstcp>
{
public:
	typedef IcsConnection<icstcp>	_baseType;
	typedef _baseType::socket		socket;
	typedef socket::shutdown_type	shutdown_type;

	IcsCenter(IcsPorxyServer& localServer, socket&& s);

	virtual ~IcsCenter();

	/// ����ײ���Ϣ
	virtual void handle(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	/// ����ƽ����Ϣ
	virtual void dispatch(ProtocolStream& request) throw(IcsException, otl_exception);

	/// ����
	virtual void error();
private:
	/// ������֤����1
	void handleAuthrize1(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	/// ������֤����2
	void handleAuthrize2(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	/// ת����Ϣ���ն�
	void handleForwardToTermianl(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

private:
	IcsPorxyServer&	m_proxyServer;
};



/// ���������
class IcsPorxyServer {
public:
	/// �����׽���
	typedef icstcp::socket socket;

	/// ����ָ��
	typedef std::shared_ptr<IcsConnection<icstcp>> ConneciontPrt;

	IcsPorxyServer(asio::io_service& ioService
		, const string& terminalAddr, std::size_t terminalMaxCount
//		, const string& webAddr, std::size_t webMaxCount
		, const string& icsCenterAddr, std::size_t icsCenterCount);

	~IcsPorxyServer();

	/// �������֤�ն�
	void addTerminalClient(const string& conID, ConneciontPrt conn);

	/// �Ƴ�����֤�ն�
	void removeTerminalClient(const string& conID);

	/// ���������ն�
	ConneciontPrt findTerminalClient(const string& conID);

	/// ���ն˷�������
	bool sendToTerminalClient(const string& conID, ProtocolStream& request);

	/// ���ICS���ķ�������
	void addIcsCenterConn(ConneciontPrt conn);

	/// �Ƴ�ICS���ķ�������
	void removeIcsCenterConn(ConneciontPrt conn);


	/// ��ȫ��ICS���ķ�������
	void sendToIcsCenter(ProtocolStream& request);

	/// ��ȡ���õ�����ʱ��
	inline uint16_t getHeartbeatTime() const 
	{
		return m_heartbeatTime;
	}

private:
	/// �ն˳�ʱ����
	void connectionTimeoutHandler(ConneciontPrt conn);


private:
	asio::io_service& m_ioService;

	// �ն˷���
	TcpServer	m_terminalTcpServer;
	std::size_t m_terminalMaxCount;
	std::unordered_map<std::string, ConneciontPrt> m_terminalConnMap;
	std::mutex	m_terminalConnMapLock;
	uint16_t		m_heartbeatTime;


	// web����
//	TcpServer	m_webTcpServer;
//	std::size_t m_webMaxCount;

	// ics���ķ���
	TcpServer	m_icsCenterTcpServer;
	std::size_t m_icsCenterMaxCount;
	std::set<ConneciontPrt> m_icsCenterConnMap;
	std::mutex	m_icsCenterConnMapLock;

//	Timer m_timer;
	TimingWheel<64> m_timer;
};

}

#endif///_ICS_PROXY_SERVER_H