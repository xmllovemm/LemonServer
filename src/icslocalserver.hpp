


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
	virtual void handle(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// ����ƽ����Ϣ
	virtual void dispatch(ProtocolStream& request) throw(IcsException, otl_exception);

	// ������
	virtual void error() throw();
private:
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

	// GPS�ϱ�
	void handleGpsReport(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);
	
	// �ն˻�Ӧ������ѯ
	void handleParamQueryResponse(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// �ն������ϱ������޸�
	void handleParamAlertReport(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// �ն˻�Ӧ�����޸�
	void handleParamModifyResponse(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// �ն˷���ʱ��ͬ������
	void handleDatetimeSync(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// �ն��ϱ���־
	void handleLogReport(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);
	
	// �ն˷�������������
	void handleHeartbeat(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

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

protected:
	IcsLocalServer&			m_localServer;
	/// ��������(��ӦICSϵͳ�м�����)
	std::string             m_connName;
	/// ����ID
	std::string				m_gwid;
	/// �豸���ͱ��(����ʱ����ͬ�豸)
	uint16_t				m_deviceKind = 0;
	/// �������к�
	uint16_t				m_send_num = 0;
	// ��һ��ҵ����(ȥ���ظ���ҵ������)
	uint32_t				m_lastBusSerialNum = uint32_t(-1);
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
	virtual void handle(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// ����ƽ����Ϣ
	virtual void dispatch(ProtocolStream& request) throw(IcsException, otl_exception);

	// ������
	virtual void error() throw();
private:

	// ת����ICS��Ӧ�ն�
	void handleICSForward(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// ����Զ���ӷ�����
	void handleConnectRemote(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// �Ͽ�Զ���ӷ�����
	void handleDisconnectRemote(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// ת����remote��Ӧ�ն�
	void handleRemoteForward(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

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
	virtual void handle(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// ����ƽ����Ϣ
	virtual void dispatch(ProtocolStream& request) throw(IcsException, otl_exception);

	// ����
	virtual void error() throw();

	// ������֤�������
	void requestAuthrize();

	// ����������Ϣ
	void sendHeartbeat();
private:
	// ����Զ��ID��Ӧ�ı���ID
	const string& findLocalID(const string& remoteID);

	// ������֤������
	void handleAuthResponse(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// ���������ת�����
	void handleForwardResponse(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);
	
	// �����������������Ϣ
	void handleOnoffLine(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// ����������ն˵���Ϣ
	void handleTerminalMessage(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

private:
	std::string		m_enterpriseID;
	bool			m_isLegal;
	std::unordered_map<std::string, std::string> m_remoteIdToDeviceIdMap;
	std::mutex m_remoteIdToDeviceIdMapLock;
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
	void addTerminalClient(const string& gwid, ConneciontPrt conn);

	/// �Ƴ�����֤�ն˶���
	void removeTerminalClient(const string& gwid);

	/// ��ѯ�ն�����
	ConneciontPrt findTerminalClient(const string& gwid);


	/// ���Զ�̴������������
	void addRemotePorxy(const string& remoteID, ConneciontPrt conn);

	/// �Ƴ�Զ�̴��������
	void removeRemotePorxy(const string& remoteID);

	/// ��ѯԶ�˷�����
	ConneciontPrt findRemoteProxy(const string& remoteID);
private:
	/// ��ʼ�����ݿ�������Ϣ
	void clearConnectionInfo();

	/// ���ӳ�ʱ����
	void connectionTimeoutHandler(ConneciontPrt conn);

	/// ���ִ������������
	void keepHeartbeat(ConneciontPrt conn);
private:
	friend class IcsTerminalClient;
	friend class IcsWebClient;

	asio::io_service& m_ioService;

	// �ն˷���
	TcpServer	m_terminalTcpServer;
	std::size_t m_terminalMaxCount;
	std::unordered_map<std::string, ConneciontPrt> m_terminalConnMap;	// gwidΪkey�����Ӷ���Ϊvalue
	std::mutex	m_terminalConnMapLock;

	// ������Ϣ
	std::string		m_onlineIP;
	int				m_onlinePort;
	uint16_t		m_heartbeatTime;

	// web����
	TcpServer	m_webTcpServer;
	std::size_t m_webMaxCount;

	// Զ�˴������
	std::unordered_map<std::string, ConneciontPrt> m_proxyConnMap;	// ��ҵIDΪkey�����Ӷ���Ϊvalue
	std::mutex	m_proxyConnMapLock;

	// ����ϵͳ
	PushSystem	m_pushSystem;
	
	TimingWheel<64>	m_timer;
};

}

#endif	// _ICS_LOCAL_SERVER_HPP