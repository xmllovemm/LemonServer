


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


/// 终端协议处理类
class IcsTerminalClient : public IcsConnection<icstcp>
{
public:
	typedef IcsConnection<icstcp>	_baseType;
	typedef _baseType::socket		socket;
	typedef socket::shutdown_type	shutdown_type;

	IcsTerminalClient(IcsLocalServer& localServer, socket&& s, const char* name = nullptr);

	virtual ~IcsTerminalClient();

	// 处理底层消息
	virtual void handle(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// 处理平层消息
	virtual void dispatch(ProtocolStream& request) throw(IcsException, otl_exception);

	// 出错处理
	virtual void error() throw();
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
	/// 链接名称(对应ICS系统中监测点编号)
	std::string             m_connName;
	/// 网关ID
	std::string				m_gwid;
	/// 设备类型编号(推送时区别不同设备)
	uint16_t				m_deviceKind = 0;
	/// 发送序列号
	uint16_t				m_send_num = 0;
	// 上一次业务编号(去除重复的业务数据)
	uint32_t				m_lastBusSerialNum = uint32_t(-1);
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

	// 出错处理
	virtual void error() throw();
private:

	// 转发到ICS对应终端
	void handleICSForward(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// 连接远端子服务器
	void handleConnectRemote(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// 断开远端子服务器
	void handleDisconnectRemote(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// 转发到remote对应终端
	void handleRemoteForward(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

private:
	IcsLocalServer& m_localServer;
	std::string		m_name;
};


/// 子服务器服务端协议处理类
class IcsRemoteProxyClient : public IcsTerminalClient
{
public:
	typedef IcsTerminalClient		_baseType;
	typedef _baseType::socket		socket;
	typedef socket::shutdown_type	shutdown_type;

	IcsRemoteProxyClient(IcsLocalServer& localServer, socket&& s, std::string enterpriseID);

	~IcsRemoteProxyClient();

	// 处理底层消息
	virtual void handle(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// 处理平层消息
	virtual void dispatch(ProtocolStream& request) throw(IcsException, otl_exception);

	// 出错
	virtual void error() throw();

	// 请求验证中心身份
	void requestAuthrize();

	// 发送心跳消息
	void sendHeartbeat();
private:
	// 查找远程ID对应的本地ID
	const string& findLocalID(const string& remoteID);

	// 处理认证请求结果
	void handleAuthResponse(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// 代理服务器转发结果
	void handleForwardResponse(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);
	
	// 代理服务器上下线消息
	void handleOnoffLine(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// 代理服务器终端的消息
	void handleTerminalMessage(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

private:
	std::string		m_enterpriseID;
	bool			m_isLegal;
	std::unordered_map<std::string, std::string> m_remoteIdToDeviceIdMap;
	std::mutex m_remoteIdToDeviceIdMapLock;
};


/// 中心服务器：监听端口、处理终端数据、链接远端代理服务器、处理远端终端数据
class IcsLocalServer {
public:
	typedef icstcp::socket socket;

	typedef std::shared_ptr<IcsConnection<icstcp>> ConneciontPrt;

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

	/// 停止事件
	void stop();

	/// 添加已认证终端对象
	void addTerminalClient(const string& gwid, ConneciontPrt conn);

	/// 移除已认证终端对象
	void removeTerminalClient(const string& gwid);

	/// 查询终端链接
	ConneciontPrt findTerminalClient(const string& gwid);


	/// 添加远程代理服务器对象
	void addRemotePorxy(const string& remoteID, ConneciontPrt conn);

	/// 移除远程代理服务器
	void removeRemotePorxy(const string& remoteID);

	/// 查询远端服务器
	ConneciontPrt findRemoteProxy(const string& remoteID);
private:
	/// 初始化数据库连接信息
	void clearConnectionInfo();

	/// 连接超时处理
	void connectionTimeoutHandler(ConneciontPrt conn);

	/// 保持代理服务器心跳
	void keepHeartbeat(ConneciontPrt conn);
private:
	friend class IcsTerminalClient;
	friend class IcsWebClient;

	asio::io_service& m_ioService;

	// 终端服务
	TcpServer	m_terminalTcpServer;
	std::size_t m_terminalMaxCount;
	std::unordered_map<std::string, ConneciontPrt> m_terminalConnMap;	// gwid为key，链接对象为value
	std::mutex	m_terminalConnMapLock;

	// 链接信息
	std::string		m_onlineIP;
	int				m_onlinePort;
	uint16_t		m_heartbeatTime;

	// web服务
	TcpServer	m_webTcpServer;
	std::size_t m_webMaxCount;

	// 远端代理服务
	std::unordered_map<std::string, ConneciontPrt> m_proxyConnMap;	// 企业ID为key，链接对象为value
	std::mutex	m_proxyConnMapLock;

	// 推送系统
	PushSystem	m_pushSystem;
	
	TimingWheel<64>	m_timer;
};

}

#endif	// _ICS_LOCAL_SERVER_HPP