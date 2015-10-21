

#ifndef _ICS_CLIENT_H
#define _ICS_CLIENT_H

#include "config.h"
#include "icsprotocol.h"
#include "tcpserver.h"
#include <string>
#include <map>
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

class IcsSimulateClient : public std::enable_shared_from_this<IcsSimulateClient>
{
public:

	IcsSimulateClient(asio::ip::tcp::socket&& s, AuthrizeInfo&& info);

	IcsSimulateClient(IcsSimulateClient&& rhs);

	IcsSimulateClient(const IcsSimulateClient& rhs) = delete;

//	IcsSimulateClient(asio::ip::tcp::socket&& s, const AuthrizeInfo& info);

	void start(asio::ip::tcp::resolver::iterator endpoint_iter);

private:
	void do_authrize();

	void do_read();

	void do_write();

	void do_close();

	void do_handle_msg(uint8_t* buf, size_t length);

private:
	asio::ip::tcp::socket m_socket;
	AuthrizeInfo	m_authrize_info;

	uint16_t		m_send_num;
	uint8_t			m_recv_buf[512];
	list<IcsProtocol> m_send_list;
};

//----------------------------------------------------------------------------------//



}	// end namespace ics
#endif	// end _ICS_CLIENT_H
