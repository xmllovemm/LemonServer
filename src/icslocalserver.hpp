


#ifndef _ICS_LOCAL_SERVER_HPP
#define _ICS_LOCAL_SERVER_HPP

#include "icsconnection.hpp"
#include "tcpserver.hpp"
#include "icspushsystem.hpp"
#include "timer.hpp"
#include <string>

using namespace std;

namespace ics {

class IcsLocalServer;

#define UPGRADE_FILE_SEGMENG_SIZE 1024

/// �ն�Э�鴦����
class IcsTerminalClient : public IcsConnection<icstcp>
{
public:
	typedef IcsConnection<icstcp>	_baseType;
	typedef _baseType::socket		socket;
	typedef socket::shutdown_type	shutdown_type;

	IcsTerminalClient(IcsLocalServer& localServer, socket&& s, const char* name = nullptr);

	virtual ~IcsTerminalClient();

	// ����ײ���Ϣ
	virtual void handle(IcsProtocolStreamInput& request, IcsProtocolStreamOutput& response) throw(IcsException, otl_exception);

	// ����ƽ����Ϣ
	virtual void dispatch(IcsProtocolStreamInput& request) throw(IcsException, otl_exception);

private:
	// �ն���֤
	void handleAuthRequest(IcsProtocolStreamInput& request, IcsProtocolStreamOutput& response) throw(IcsException, otl_exception);

	// ��׼״̬�ϱ�
	void handleStdStatusReport(IcsProtocolStreamInput& request, IcsProtocolStreamOutput& response) throw(IcsException, otl_exception, otl_exception);

	// �Զ���״̬�ϱ�
	void handleDefStatusReport(IcsProtocolStreamInput& request, IcsProtocolStreamOutput& response) throw(IcsException, otl_exception);

	// �¼��ϱ�
	void handleEventsReport(IcsProtocolStreamInput& request, IcsProtocolStreamOutput& response) throw(IcsException, otl_exception);
	
	// ҵ���ϱ�
	void handleBusinessReport(IcsProtocolStreamInput& request, IcsProtocolStreamOutput& response)  throw(IcsException, otl_exception);

	// GPS�ϱ�
	void handleGpsReport(IcsProtocolStreamInput& request, IcsProtocolStreamOutput& response) throw(IcsException, otl_exception);
	
	// �ն˻�Ӧ������ѯ
	void handleParamQueryResponse(IcsProtocolStreamInput& request, IcsProtocolStreamOutput& response) throw(IcsException, otl_exception);

	// �ն������ϱ������޸�
	void handleParamAlertReport(IcsProtocolStreamInput& request, IcsProtocolStreamOutput& response) throw(IcsException, otl_exception);

	// �ն˻�Ӧ�����޸�
	void handleParamModifyResponse(IcsProtocolStreamInput& request, IcsProtocolStreamOutput& response) throw(IcsException, otl_exception);

	// �ն˷���ʱ��ͬ������
	void handleDatetimeSync(IcsProtocolStreamInput& request, IcsProtocolStreamOutput& response) throw(IcsException, otl_exception);

	// �ն��ϱ���־
	void handleLogReport(IcsProtocolStreamInput& request, IcsProtocolStreamOutput& response) throw(IcsException, otl_exception);
	
	// �ն˷�������������
	void handleHeartbeat(IcsProtocolStreamInput& request, IcsProtocolStreamOutput& response) throw(IcsException, otl_exception);

	// �ն˾ܾ���������
	void handleDenyUpgrade(IcsProtocolStreamInput& request, IcsProtocolStreamOutput& response) throw(IcsException, otl_exception);
	
	// �ն˽�����������
	void handleAgreeUpgrade(IcsProtocolStreamInput& request, IcsProtocolStreamOutput& response) throw(IcsException, otl_exception);
	
	// ��Ҫ�����ļ�Ƭ��
	void handleRequestFile(IcsProtocolStreamInput& request, IcsProtocolStreamOutput& response) throw(IcsException, otl_exception);

	// �����ļ�������
	void handleUpgradeResult(IcsProtocolStreamInput& request, IcsProtocolStreamOutput& response) throw(IcsException, otl_exception);

	// �ն�ȷ��ȡ������
	void handleUpgradeCancelAck(IcsProtocolStreamInput& request, IcsProtocolStreamOutput& response) throw(IcsException, otl_exception);

protected:
	IcsLocalServer&			m_localServer;
	std::string             m_connName;
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

};


/// webЭ�鴦����
class IcsWebClient : public IcsConnection<icstcp>
{
public:
	typedef IcsConnection<icstcp>		_baseType;
	typedef _baseType::socket		socket;
	typedef socket::shutdown_type	shutdown_type;

	IcsWebClient(IcsLocalServer& localServer, socket&& s);

	// ����ײ���Ϣ
	virtual void handle(IcsProtocolStreamInput& request, IcsProtocolStreamOutput& response) throw(IcsException, otl_exception);

	// ����ƽ����Ϣ
	virtual void dispatch(IcsProtocolStreamInput& request) throw(IcsException, otl_exception);
private:

	// ת������Ӧ�ն�
	void handleForward(IcsProtocolStreamInput& request, IcsProtocolStreamOutput& response) throw(IcsException, otl_exception);

	// ����Զ���ӷ�����
	void handleConnectRemote(IcsProtocolStreamInput& request, IcsProtocolStreamOutput& response) throw(IcsException, otl_exception);

	// �Ͽ�Զ���ӷ�����
	void handleDisconnectRemote(IcsProtocolStreamInput& request, IcsProtocolStreamOutput& response) throw(IcsException, otl_exception);

private:
	IcsLocalServer& m_localServer;
	std::string		m_name;
};


/// �ӷ����������Э�鴦����
class IcsRemoteProxyClient : public IcsTerminalClient
{
public:
	typedef IcsTerminalClient		_baseType;
	typedef _baseType::socket		socket;
	typedef socket::shutdown_type	shutdown_type;

	IcsRemoteProxyClient(IcsLocalServer& localServer, socket&& s, std::string enterpriseID);

	~IcsRemoteProxyClient();

	// ����ײ���Ϣ
	virtual void handle(IcsProtocolStreamInput& request, IcsProtocolStreamOutput& response) throw(IcsException, otl_exception);

	// ����ƽ����Ϣ
	virtual void dispatch(IcsProtocolStreamInput& request) throw(IcsException, otl_exception);

	// ������֤�������
	void requestAuthrize();
private:
	// ������֤������
	void handleAuthResponse(IcsProtocolStreamInput& request, IcsProtocolStreamOutput& response) throw(IcsException, otl_exception);

	// ������������ת������Ϣ
	void handleForwardMessage(IcsProtocolStreamInput& request, IcsProtocolStreamOutput& response) throw(IcsException, otl_exception);

private:
	std::string		m_enterpriseID;

	static std::unordered_map<std::string, std::string> s_remoteIdToDeviceIdMap;
	static std::mutex s_idMapLock;
};


/// ���ķ������������˿ڡ������ն����ݡ�����Զ�˴��������������Զ���ն�����
class IcsLocalServer {
public:
	typedef icstcp::socket socket;

	typedef std::shared_ptr<IcsConnection<icstcp>> ConneciontPrt;

	/*
	param ioService: io�������
	param terminalAddr: �ն˼�����ַ
	param terminalMaxCount: �ն������������ֵ
	param webAddr: web��˼�����ַ
	param webMaxCount: web�����������ֵ
	*/
	IcsLocalServer(asio::io_service& ioService
		, const string& terminalAddr, std::size_t terminalMaxCount
		, const string& webAddr, std::size_t webMaxCount
		, const string& pushAddr);

	~IcsLocalServer();

	/// �����¼�ѭ��
	void start();

	/// ֹͣ�¼�
	void stop();

	/// �������֤�ն˶���
	void addTerminalClient(const string& conID, ConneciontPrt conn);

	/// �Ƴ�����֤�ն˶���
	void removeTerminalClient(const string& conID);

	/// �������ݵ��ն˶���
	bool sendToTerminalClient(const string& conID, IcsProtocolStreamInput& request);

	/// ���Զ�̴������������
	void addRemotePorxy(const string& remoteID, ConneciontPrt conn);

	/// �Ƴ�Զ�̴��������
	void removeRemotePorxy(const string& remoteID);

private:
	friend class IcsTerminalClient;
	friend class IcsWebClient;

	asio::io_service& m_ioService;

	// �ն˷���
	TcpServer	m_terminalTcpServer;
	std::size_t m_terminalMaxCount;
	std::unordered_map<std::string, ConneciontPrt> m_terminalConnMap;
	std::mutex	m_terminalConnMapLock;

	// ������Ϣ
	std::string		m_onlineIP;
	int				m_onlinePort;
	uint16_t		m_heartbeatTime;

	// web����
	TcpServer	m_webTcpServer;
	std::size_t m_webMaxCount;

	// Զ�˴������
	std::unordered_map<std::string, ConneciontPrt> m_proxyConnMap;
	std::mutex	m_proxyConnMapLock;

	// ����ϵͳ
	PushSystem	m_pushSystem;
	
	Timer		m_timer;
};

}

#endif	// _ICS_LOCAL_SERVER_HPP