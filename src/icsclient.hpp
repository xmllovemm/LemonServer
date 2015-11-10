

#ifndef _ICS_CLIENT_H
#define _ICS_CLIENT_H

#include "config.hpp"
#include "icsprotocol.hpp"
#include "tcpconnection.hpp"
#include "clientmanager.hpp"
#include "mempool.hpp"
#include <string>
#include <map>
#include <list>
#include <thread>

using namespace std;

namespace ics{


typedef protocol::IcsMsgHead<uint32_t, false> ProtocolHead;
typedef protocol::ProtocolStream<ProtocolHead> ProtocolStream;

class ClientHandler;
class TerminalClient;

//---------------------------------IcsConnection-------------------------------------------------//
class IcsConnection : public TcpConnection {
public:

	IcsConnection(asio::ip::tcp::socket&& s, ClientManager& cm);

    virtual ~IcsConnection();
    
	IcsConnection(IcsConnection&& rhs) = delete;

	IcsConnection(const IcsConnection& rhs) = delete;

	virtual void start();
protected:
    virtual void do_read();
    
    virtual void do_write();
    
private:

	void toHexInfo(const char* info, uint8_t* buf, std::size_t length);

	void trySend(MemoryChunk& mc);

	void trySend();

	void replyResponse(uint16_t ackNum);

	bool handleData(uint8_t* buf, std::size_t length);
	
private:
	ClientHandler*			m_clientHandler;
	std::string				m_connectionID;
	std::string             m_conn_name;
	uint16_t				m_deviceKind;
	ClientManager&			m_client_manager;

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

//---------------------------------TerminalClient-------------------------------------------------//

class ClientHandler {
public:
	ClientHandler(IcsConnection& client)
	{

	}

	virtual ~ClientHandler()
	{

	}

	// 处理对端数据
	virtual void handle(ProtocolStream& request, ProtocolStream& response) throw(std::runtime_error) = 0;

	// 转发消息到对端
	virtual void dispatch(ProtocolStream& request, ProtocolStream& response) throw(std::runtime_error) = 0;

	// 获取客户端ID
	virtual std::string& name() = 0;
};

// 终端连接处理类
class TerminalClient : public ClientHandler {
public:
	TerminalClient(IcsConnection& client);

	
	virtual void handle(ProtocolStream& request, ProtocolStream& response) throw(std::runtime_error);

	
	virtual void dispatch(ProtocolStream& request, ProtocolStream& response) throw(std::runtime_error);

	virtual std::string& name();
private:
	// handle data from remote terminal report
	void handleFromTerminal(ProtocolStream& request, ProtocolStream& response);

	// handle data from remote web command
	void handleFromWeb(ProtocolStream& request, ProtocolStream& response);

	// handle data from another sub-server
	void handleFromAnother(ProtocolStream& request, ProtocolStream& response);

private:
	// 终端认证
	void handleAuthRequest(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error);

	// 标准状态上报
	void handleStdStatusReport(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error);

	// 自定义状态上报
	void handleDefStatusReport(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error);

	// 事件上报
	void handleEventsReport(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error);

	// 终端回应参数查询
	void handleParamQueryResponse(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error);

	// 终端主动上报参数修改
	void handleParamAlertReport(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error);

	// 终端回应参数修改
	void handleParamModifyResponse(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error);

	// 业务上报
	void handleBusinessReport(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error);

	// GPS上报
	void handleGpsReport(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error);

	// 终端发送时钟同步请求
	void handleDatetimeSync(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error);

	// 终端上报日志
	void handleLogReport(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error);

	// 终端发送心跳到中心
	void handleHeartbeat(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error);

	// 终端拒绝升级请求
	void handleDenyUpgrade(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error);

	// 终端接收升级请求
	void handleAgreeUpgrade(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error);

	// 索要升级文件片段
	void handleRequestFile(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error);

	// 升级文件传输结果
	void handleUpgradeResult(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error);

	// 终端确认取消升级
	void handleUpgradeCancelAck(ProtocolStream& request, ProtocolStream& response) throw(std::logic_error);


private:
	std::string             m_conn_name;
	uint16_t				m_deviceKind;
	ClientManager&			m_client_manager;

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

//---------------------------------WebClient-------------------------------------------------//
// Web连接处理类
class WebClient : public ClientHandler {
public:
	WebClient();

	// 取出监测点名,找到对应对象转发该消息/文件传输处理
	void handle(ProtocolStream& request, ProtocolStream& response) throw(std::runtime_error);

};

}	// end namespace ics
#endif	// end _ICS_CLIENT_H
