

#ifndef _ICS_CLIENT_H
#define _ICS_CLIENT_H

#include "config.hpp"
#include "icsprotocol.hpp"
#include "tcpconnection.hpp"
#include "clientmanager.hpp"
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

public:
    virtual void do_read();
    
    virtual void do_write();
    
private:
	bool do_handle_msg(uint8_t* buf, size_t length);

	void debug_msg(uint8_t* buf, size_t length);

    void do_authrize(IcsProtocol& proto);
    
	// 终端认证
	void handleAuthRequest(IcsProtocol& ip) throw(std::logic_error);

	// 标准状态上报
	void handleStdStatusReport(IcsProtocol& proto) throw(std::logic_error);

	// 自定义状态上报
	void handleDefStatusReport(IcsProtocol& proto) throw(std::logic_error);

	// 事件上报
	void handleEventsReport(IcsProtocol& proto) throw(std::logic_error);

	// 终端回应参数查询
	void handleParamQueryResponse(IcsProtocol& proto) throw(std::logic_error);

	// 终端主动上报参数修改
	void handleParamAlertReport(IcsProtocol& proto) throw(std::logic_error);

	// 终端回应参数修改
	void handleParamModifyResponse(IcsProtocol& proto) throw(std::logic_error);

	// 业务上报
	void handleBusinessReport(IcsProtocol& proto) throw(std::logic_error);

	// 终端发送时钟同步请求
	void handleDatetimeSync(IcsProtocol& proto) throw(std::logic_error);

	// 终端上报日志
	void handleLogReport(IcsProtocol& proto) throw(std::logic_error);

	// 终端发送心跳到中心
	void handleHeartbeat(IcsProtocol& proto) throw(std::logic_error);

	// 终端拒绝升级请求
	void handleDenyUpgrade(IcsProtocol& proto) throw(std::logic_error);

	// 终端接收升级请求
	void handleAgreeUpgrade(IcsProtocol& proto) throw(std::logic_error);

	// 索要升级文件片段
	void handleRequestFile(IcsProtocol& proto) throw(std::logic_error);

	// 升级文件传输结果
	void handleUpgradeResult(IcsProtocol& proto) throw(std::logic_error);

	// 终端确认取消升级
	void handleUpgradeCancelAck(IcsProtocol& proto) throw(std::logic_error);

private:
	// recv area
	uint16_t		m_send_num;
	uint8_t			m_recv_buf[512];
	IcsProtocol		m_ics_protocol;

	// send area
    std::list<IcsProtocol> m_send_list;
};

//----------------------------------------------------------------------------------//



}	// end namespace ics
#endif	// end _ICS_CLIENT_H
