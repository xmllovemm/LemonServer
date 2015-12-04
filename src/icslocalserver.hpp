


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

/// 终端协议处理类
class IcsTerminalClient : public IcsConnection<icstcp>
{
public:
	typedef IcsConnection<icstcp>	_baseType;
	typedef _baseType::socket		socket;
	typedef socket::shutdown_type	shutdown_type;

	IcsTerminalClient(IcsLocalServer& localServer, socket&& s);

	virtual ~IcsTerminalClient();

	// 处理底层消息
	virtual void handle(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// 处理平层消息
	virtual void dispatch(ProtocolStream& request) throw(IcsException, otl_exception);

private:
	// 终端认证
	void handleAuthRequest(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// 标准状态上报
	void handleStdStatusReport(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception, otl_exception);

	// 自定义状态上报
	void handleDefStatusReport(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// 事件上报
	void handleEventsReport(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);
	
	// 业务上报
	void handleBusinessReport(ProtocolStream& request, ProtocolStream& response)  throw(IcsException, otl_exception);

	// GPS上报
	void handleGpsReport(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);
	
	// 终端回应参数查询
	void handleParamQueryResponse(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// 终端主动上报参数修改
	void handleParamAlertReport(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// 终端回应参数修改
	void handleParamModifyResponse(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// 终端发送时钟同步请求
	void handleDatetimeSync(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// 终端上报日志
	void handleLogReport(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);
	
	// 终端发送心跳到中心
	void handleHeartbeat(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// 终端拒绝升级请求
	void handleDenyUpgrade(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);
	
	// 终端接收升级请求
	void handleAgreeUpgrade(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);
	
	// 索要升级文件片段
	void handleRequestFile(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// 升级文件传输结果
	void handleUpgradeResult(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// 终端确认取消升级
	void handleUpgradeCancelAck(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

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


/// web协议处理类
class IcsWebClient : public IcsConnection<icstcp>
{
public:
	typedef IcsConnection<icstcp>		_baseType;
	typedef _baseType::socket		socket;
	typedef socket::shutdown_type	shutdown_type;

	IcsWebClient(IcsLocalServer& localServer, socket&& s);

	// 处理底层消息
	virtual void handle(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// 处理平层消息
	virtual void dispatch(ProtocolStream& request) throw(IcsException, otl_exception);

	// 链接名称
	virtual const std::string& name() const;
private:

	// 转发到对应终端
	void handleForward(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// 连接远端子服务器
	void handleConnectRemote(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// 断开远端子服务器
	void handleDisconnectRemote(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

private:
	IcsLocalServer& m_localServer;
	std::string		m_name;
};


/// 子服务器服务端协议处理类
class IcsProxyClient : public IcsTerminalClient
{
public:
	typedef IcsTerminalClient		_baseType;
	typedef _baseType::socket		socket;
	typedef socket::shutdown_type	shutdown_type;

	IcsProxyClient(IcsLocalServer& localServer, socket&& s, std::string enterpriseID);

	// 处理底层消息
	virtual void handle(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// 处理平层消息
	virtual void dispatch(ProtocolStream& request) throw(IcsException, otl_exception);

	// 请求验证中心身份
	void questAuthrize();
private:
	// 处理认证请求结果
	void handleAuthResponse(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

//	IcsTerminalClient		m_icsTerminal;
	std::string		m_enterpriseID;
};


/// 中心服务器：监听端口、处理终端数据、链接远端代理服务器、处理远端终端数据
class IcsLocalServer {
public:
	typedef icstcp::socket socket;

	typedef std::unique_ptr<IcsConnection<icstcp>> ConneciontPrt;

	/*
	param ioService: io服务核心
	param terminalAddr: 终端监听地址
	param terminalMaxCount: 终端链接数量最大值
	param webAddr: web后端监听地址
	param webMaxCount: web链接数量最大值
	*/
	IcsLocalServer(asio::io_service& ioService
		, const string& terminalAddr, std::size_t terminalMaxCount
		, const string& webAddr, std::size_t webMaxCount
		, const string& pushAddr);

	~IcsLocalServer();

	/// 开启事件循环
	void start();

	/// 添加已认证终端对象
	void addTerminalClient(const string& conID, ConneciontPrt conn);

	/// 移除已认证终端对象
	void removeTerminalClient(const string& conID);

	/// 发送数据到终端对象
	bool sendToTerminalClient(const string& conID, ProtocolStream& request);

	/// 添加远程代理服务器对象
	void addRemotePorxy(const string& remoteID, ConneciontPrt conn);

	/// 移除远程代理服务器
	void removeRemotePorxy(const string& remoteID);

private:
	friend class IcsTerminalClient;
	friend class IcsWebClient;

	asio::io_service& m_ioService;

	// 终端服务
	TcpServer	m_terminalTcpServer;
	std::size_t m_terminalMaxCount;
	std::unordered_map<std::string, ConneciontPrt> m_terminalConnMap;
	std::mutex	m_terminalConnMapLock;

	// 链接信息
	std::string		m_onlineIP;
	int				m_onlinePort;
	uint16_t		m_heartbeatTime;

	// web服务
	TcpServer	m_webTcpServer;
	std::size_t m_webMaxCount;

	// 远端代理服务
	std::unordered_map<std::string, ConneciontPrt> m_proxyConnMap;
	std::mutex	m_proxyConnMapLock;

	// 推送系统
	PushSystem	m_pushSystem;
	
	Timer		m_timer;
};

}

#endif	// _ICS_LOCAL_SERVER_HPP