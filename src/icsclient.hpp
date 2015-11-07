

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

//----------------------------------------------------------------------------------//

class AuthrizeInfo
{
public:
	friend class IcsSimulateClient;

	AuthrizeInfo() = delete;

	AuthrizeInfo(const AuthrizeInfo& rhs);

	AuthrizeInfo(AuthrizeInfo&& rhs);

	AuthrizeInfo(const string& gw_id, const string& gw_pwd, uint16_t device_kind, const string& ext_info);

	AuthrizeInfo(string&& gw_id, string&& gw_pwd, uint16_t device_kind, string&& ext_info);

	/*
	AuthrizeInfo(const char* gw_id, const char* gw_pwd, uint16_t device_kind, const char* ext_info);
	*/

	AuthrizeInfo& operator=(const AuthrizeInfo& rhs);

	AuthrizeInfo& operator=(AuthrizeInfo&& rhs);

	friend ostream& operator<<(ostream& os, const AuthrizeInfo& rhs);	// not a member function

public:
	string m_gw_id;
	string m_gw_pwd;
	uint16_t m_device_kind;
	string m_ext_info;
};

//----------------------------------------------------------------------------------//

class IcsClient : public TcpConnection {
public:

	IcsClient(asio::ip::tcp::socket&& s, ClientManager& cm);

    virtual ~IcsClient();
    
	IcsClient(IcsClient&& rhs) = delete;

	IcsClient(const IcsClient& rhs) = delete;

protected:
    virtual void do_read();
    
    virtual void do_write();
    
private:
	typedef protocol::IcsMsgHead<uint32_t,false> ProtocolHead;

	typedef protocol::ProtocolStream<ProtocolHead> ProtocolStream;

	void toHexInfo(uint8_t* buf, std::size_t length);

	void trySend(MemoryChunk& mc);

	void trySend();

	bool handleData(uint8_t* buf, std::size_t length);

	void handleMessage(ProtocolStream& request, ProtocolStream& response);

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

//----------------------------------------------------------------------------------//

#include "log.hpp"
class SubCommServerClient : public TcpConnection {
public:
	SubCommServerClient(asio::io_service& io)
		: TcpConnection(asio::ip::tcp::socket(io))
	{

	}

	void connectTo(const string& ip, int port)
	{
		m_socket.async_connect(asio::ip::tcp::endpoint(asio::ip::address::from_string(ip), port), [this](const asio::error_code& ec)
			{
				if (ec)
				{
					LOG_DEBUG("connect to server failed");
				}
				else
				{
					reportHearBeat();
				}
			});
		
	}

	virtual void do_read()
	{

	}

	virtual void do_write()
	{

	}

private:
	void reportHearBeat()
	{
		char buff[126];
		protocol::ProtocolStream<protocol::IcsMsgHead<uint16_t>> pp(buff, sizeof(buff));

	}
};

}	// end namespace ics
#endif	// end _ICS_CLIENT_H
