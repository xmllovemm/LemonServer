
#ifndef _ICS_PROXY_SERVER_H
#define _ICS_PROXY_SERVER_H

#include "icsconnection.hpp"
#include "tcpserver.hpp"
#include <unordered_map>
#include <mutex>


namespace ics {

class IcsPorxyServer;

/// 终端协议处理类
class IcsProxyTerminalClient : public IcsConnection<icstcp>
{
public:
	typedef IcsConnection<icstcp>	_baseType;
	typedef _baseType::socket		socket;
	typedef socket::shutdown_type	shutdown_type;

	IcsProxyTerminalClient(IcsPorxyServer& localServer, socket&& s);

	virtual ~IcsProxyTerminalClient();

	// 处理底层消息
	virtual void handle(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// 处理平层消息
	virtual void dispatch(ProtocolStream& request) throw(IcsException, otl_exception);

private:
	// 终端认证
	void handleAuthRequest(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// 事件上报
	void handleEventsReport(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// 业务上报
	void handleBusinessReport(ProtocolStream& request, ProtocolStream& response)  throw(IcsException, otl_exception);
	
	// 终端发送心跳到中心
	void handleHeartbeat(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);
	
	// 终端发送时钟同步请求
	void handleDatetimeSync(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

private:
	IcsPorxyServer&	m_proxyServer;

	std::string             m_connName;
	uint16_t				m_deviceKind;
	uint16_t		m_send_num;

	// business area
	uint32_t		m_lastBusSerialNum;

};

/// 远端web协议处理类
class IcsProxyWebClient : public IcsConnection<icstcp>
{
public:
	typedef IcsConnection<icstcp>	_baseType;
	typedef _baseType::socket		socket;
	typedef socket::shutdown_type	shutdown_type;

	IcsProxyWebClient(IcsPorxyServer& localServer, socket&& s);

	virtual ~IcsProxyWebClient();

	// 处理底层消息
	virtual void handle(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// 处理平层消息
	virtual void dispatch(ProtocolStream& request) throw(IcsException, otl_exception);

private:
	// 文件传输请求
	void handleTransFileRequest(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// 文件片段处理
	void handleTransFileFrament(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

private:
	IcsPorxyServer&	m_proxyServer;
};

/// 转发ICS中心处理类
class IcsProxyForward : public IcsConnection<icstcp>
{
public:
	typedef IcsConnection<icstcp>	_baseType;
	typedef _baseType::socket		socket;
	typedef socket::shutdown_type	shutdown_type;

	IcsProxyForward(IcsPorxyServer& localServer, socket&& s);

	virtual ~IcsProxyForward();

	// 处理底层消息
	virtual void handle(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// 处理平层消息
	virtual void dispatch(ProtocolStream& request) throw(IcsException, otl_exception);

private:
	IcsPorxyServer&	m_proxyServer;
};



/// 代理服务器
class IcsPorxyServer {
public:
	/// 链接套接字
	typedef icstcp::socket socket;

	/// 链接指针
	typedef std::unique_ptr<IcsConnection<icstcp>> ConneciontPrt;

	IcsPorxyServer(asio::io_service& ioService
		, const string& terminalAddr, std::size_t terminalMaxCount
		, const string& webAddr, std::size_t webMaxCount
		, const string& icsCenterAddr, std::size_t icsCenterCount);

	/// 添加已认证终端
	void addTerminalClient(const string& conID, ConneciontPrt conn);

	/// 移除已认证终端
	void removeTerminalClient(const string& conID);

	/// 向终端发送数据
	bool sendToTerminalClient(const string& conID, ProtocolStream& request);

	/// 添加ICS中心服务链接
	void addIcsCenterConn(ConneciontPrt conn);

	/// 向全部ICS中心发送数据
	void sendToIcsCenter(ProtocolStream& request);

private:
	asio::io_service& m_ioService;

	// 终端服务
	TcpServer	m_terminalTcpServer;
	std::size_t m_terminalMaxCount;
	std::unordered_map<std::string, ConneciontPrt> m_terminalConnMap;
	std::mutex	m_terminalConnMapLock;
	uint16_t		m_heartbeatTime;


	// web服务
	TcpServer	m_webTcpServer;
	std::size_t m_webMaxCount;

	// ics中心服务
	TcpServer	m_icsCenterTcpServer;
	std::size_t m_icsCenterMaxCount;
	std::list<ConneciontPrt> m_icsCenterConnMap;
	std::mutex	m_icsCenterConnMapLock;


};

}

#endif // _ICS_PROXY_SERVER_H