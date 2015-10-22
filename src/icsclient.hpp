

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
	void do_handle_msg(uint8_t* buf, size_t length);

    void do_authrize(IcsProtocol& proto);
    
private:
	//	AuthrizeInfo	m_authrize_info;

	uint16_t		m_send_num;
	uint8_t			m_recv_buf[512];
    std::list<IcsProtocol> m_send_list;
};

//----------------------------------------------------------------------------------//



}	// end namespace ics
#endif	// end _ICS_CLIENT_H
