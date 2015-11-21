

#ifndef _ICS_CLIENT_H
#define _ICS_CLIENT_H

#include "config.hpp"
#include "connection.hpp"
#include "icsprotocol.hpp"
#include "clientmanager.hpp"
#include "mempool.hpp"
#include <asio.hpp>
#include <string>
#include <unordered_map>
#include <list>
#include <thread>

using namespace std;

namespace ics{


template<class T>
class IcsConnection;

typedef IcsConnection<asio::ip::tcp> TcpConnection;


// ICS协议处理接口类
class MessageHandlerImpl {
public:
	MessageHandlerImpl() {}

	virtual ~MessageHandlerImpl(){}

	// 处理对端数据
	virtual void handle(TcpConnection& conn, ProtocolStream& request, ProtocolStream& response) throw(IcsException,otl_exception) = 0;

	// 转发消息到对端
	virtual void dispatch(TcpConnection& conn, ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception) = 0;
};


// 终端连接处理类
class TerminalHandler : public MessageHandlerImpl {
public:
	TerminalHandler();

	// 处理终端发送数据
	virtual void handle(TcpConnection& conn, ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// 将该消息转发到终端
	virtual void dispatch(TcpConnection& conn, ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

private:
	// 终端认证
	void handleAuthRequest(TcpConnection& conn, ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// 标准状态上报
	void handleStdStatusReport(TcpConnection& conn, ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// 自定义状态上报
	void handleDefStatusReport(TcpConnection& conn, ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// 事件上报
	void handleEventsReport(TcpConnection& conn, ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// 终端回应参数查询
	void handleParamQueryResponse(TcpConnection& conn, ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// 终端主动上报参数修改
	void handleParamAlertReport(TcpConnection& conn, ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// 终端回应参数修改
	void handleParamModifyResponse(TcpConnection& conn, ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// 业务上报
	void handleBusinessReport(TcpConnection& conn, ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// GPS上报
	void handleGpsReport(TcpConnection& conn, ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// 终端发送时钟同步请求
	void handleDatetimeSync(TcpConnection& conn, ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// 终端上报日志
	void handleLogReport(TcpConnection& conn, ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// 终端发送心跳到中心
	void handleHeartbeat(TcpConnection& conn, ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// 终端拒绝升级请求
	void handleDenyUpgrade(TcpConnection& conn, ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// 终端接收升级请求
	void handleAgreeUpgrade(TcpConnection& conn, ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// 索要升级文件片段
	void handleRequestFile(TcpConnection& conn, ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// 升级文件传输结果
	void handleUpgradeResult(TcpConnection& conn, ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// 终端确认取消升级
	void handleUpgradeCancelAck(TcpConnection& conn, ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);



private:
	friend class ProxyTerminalHandler;

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
};


// 中心处理子服务器消息类
class ProxyTerminalHandler : public TerminalHandler {
public:
	ProxyTerminalHandler();

	// 处理远端服务器转发的终端数据
	virtual void handle(TcpConnection& conn, ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	virtual void dispatch(TcpConnection& conn, ProtocolStream& request, ProtocolStream& response)  throw(IcsException, otl_exception);

private:
	std::unordered_map<std::string, std::string> m_nameMap;
	TerminalHandler m_handler;
};


// Web连接处理类
class WebHandler : public MessageHandlerImpl {
public:
	WebHandler();

	// 取出监测点名,找到对应对象转发该消息/文件传输处理
	virtual void handle(TcpConnection& conn, ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// never uesd
	virtual void dispatch(TcpConnection& conn, ProtocolStream& request, ProtocolStream& response)  throw(IcsException, otl_exception);

private:
	// 转发到对应终端
	void handleForward(TcpConnection& conn, ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// 连接远端子服务器
	void handleConnectRemote(TcpConnection& conn, ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// 文件传输请求
	void handleTransFileRequest(TcpConnection& conn, ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// 文件片段处理
	void handleTransFileFrament(TcpConnection& conn, ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

};


// 子服务器处理中心消息类
class CenterServerHandler : public TerminalHandler {
public:
	CenterServerHandler();

	// 处理中心消息
	virtual void handle(TcpConnection& conn, ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// 将消息转发到中心
	virtual void dispatch(TcpConnection& conn, ProtocolStream& request, ProtocolStream& response)  throw(IcsException, otl_exception);

};

}	// end namespace ics

#endif	// end _ICS_CLIENT_H
