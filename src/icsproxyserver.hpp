
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

	// 出错处理
	virtual void error() throw();

private:
	/// 转发到ICS中心
	void forwardToIcsCenter(ProtocolStream& request);

	/// 上下线通知:status 0-online,1-offline
	void onoffLineToIcsCenter(uint8_t status);

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
	
	// 终端发送心跳到中心
	void handleHeartbeat(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);
	
	// 终端发送时钟同步请求
	void handleDatetimeSync(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// GPS上报
	void handleGpsReport(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);
	
	// 终端上报日志
	void handleLogReport(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

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

private:
	IcsPorxyServer&	m_proxyServer;

	std::string     m_connName;
	uint16_t		m_deviceKind;
	uint16_t		m_send_num;

	// business area
	uint32_t		m_lastBusSerialNum;

};



/// ICS中心处理类
class IcsCenter : public IcsConnection<icstcp>
{
public:
	typedef IcsConnection<icstcp>	_baseType;
	typedef _baseType::socket		socket;
	typedef socket::shutdown_type	shutdown_type;

	IcsCenter(IcsPorxyServer& localServer, socket&& s);

	virtual ~IcsCenter();

	/// 处理底层消息
	virtual void handle(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	/// 处理平层消息
	virtual void dispatch(ProtocolStream& request) throw(IcsException, otl_exception);

	/// 出错
	virtual void error();
private:
	/// 中心认证请求1
	void handleAuthrize1(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	/// 中心认证请求2
	void handleAuthrize2(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	/// 转发消息给终端
	void handleForwardToTermianl(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

private:
	IcsPorxyServer&	m_proxyServer;
};



/// 代理服务器
class IcsPorxyServer {
public:
	/// 链接套接字
	typedef icstcp::socket socket;

	/// 链接指针
	typedef std::shared_ptr<IcsConnection<icstcp>> ConneciontPrt;

	IcsPorxyServer(asio::io_service& ioService
		, const string& terminalAddr, std::size_t terminalMaxCount
//		, const string& webAddr, std::size_t webMaxCount
		, const string& icsCenterAddr, std::size_t icsCenterCount);

	~IcsPorxyServer();

	/// 添加已认证终端
	void addTerminalClient(const string& conID, ConneciontPrt conn);

	/// 移除已认证终端
	void removeTerminalClient(const string& conID);

	/// 查找链接终端
	ConneciontPrt findTerminalClient(const string& conID);

	/// 向终端发送数据
	bool sendToTerminalClient(const string& conID, ProtocolStream& request);

	/// 添加ICS中心服务链接
	void addIcsCenterConn(ConneciontPrt conn);

	/// 移除ICS中心服务链接
	void removeIcsCenterConn(ConneciontPrt conn);


	/// 向全部ICS中心发送数据
	void sendToIcsCenter(ProtocolStream& request);

	/// 获取配置的心跳时间
	inline uint16_t getHeartbeatTime() const 
	{
		return m_heartbeatTime;
	}

private:
	/// 终端超时处理
	void connectionTimeoutHandler(ConneciontPrt conn);


private:
	asio::io_service& m_ioService;

	// 终端服务
	TcpServer	m_terminalTcpServer;
	std::size_t m_terminalMaxCount;
	std::unordered_map<std::string, ConneciontPrt> m_terminalConnMap;
	std::mutex	m_terminalConnMapLock;
	uint16_t		m_heartbeatTime;


	// web服务
//	TcpServer	m_webTcpServer;
//	std::size_t m_webMaxCount;

	// ics中心服务
	TcpServer	m_icsCenterTcpServer;
	std::size_t m_icsCenterMaxCount;
	std::set<ConneciontPrt> m_icsCenterConnMap;
	std::mutex	m_icsCenterConnMapLock;

//	Timer m_timer;
	TimingWheel<64> m_timer;
};

}

#endif///_ICS_PROXY_SERVER_H