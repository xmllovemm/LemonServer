
#include "config.h"
#include "icsclient.h"
#include "icsprotocol.h"
#include "log.h"

namespace ics{

//----------------------------------------------------------------------------------//

AuthrizeInfo::AuthrizeInfo(const AuthrizeInfo& rhs)
{
	m_gw_id = rhs.m_gw_id;
	m_gw_pwd = rhs.m_gw_pwd;
	m_ext_info = rhs.m_ext_info;
}

AuthrizeInfo::AuthrizeInfo(AuthrizeInfo&& rhs)
	: m_gw_id(rhs.m_gw_id), m_gw_pwd(rhs.m_gw_pwd), m_ext_info(rhs.m_ext_info)
{

}

AuthrizeInfo::AuthrizeInfo(const string& gw_id, const string& gw_pwd, uint16_t device_kind, const string& ext_info)
	: m_gw_id(gw_id), m_gw_pwd(gw_pwd), m_device_kind(device_kind), m_ext_info(ext_info)
{

}


AuthrizeInfo::AuthrizeInfo(string&& gw_id, string&& gw_pwd, uint16_t device_kind, string&& ext_info)
	: m_gw_id(gw_id), m_gw_pwd(gw_pwd), m_device_kind(device_kind), m_ext_info(ext_info)
{

}


AuthrizeInfo& AuthrizeInfo::operator=(const AuthrizeInfo& rhs)
{
	m_gw_id = rhs.m_gw_id;
	m_gw_pwd = rhs.m_gw_pwd;
	m_ext_info = rhs.m_ext_info;
	return *this;
}

AuthrizeInfo& AuthrizeInfo::operator=(AuthrizeInfo&& rhs)
{
	m_gw_id = rhs.m_gw_id;
	m_gw_pwd = rhs.m_gw_pwd;
	m_ext_info = rhs.m_ext_info;
	return *this;
}


ostream& operator << (ostream& os, const AuthrizeInfo& rhs)
{
	os << "GW id: " << rhs.m_gw_id << ", pwd: " << rhs.m_gw_pwd << ", ext info: " << rhs.m_ext_info;
	return os;
}

//----------------------------------------------------------------------------------//





//*/
    
IcsClient::IcsClient(asio::ip::tcp::socket&& s)
    : TcpConnection(std::move(s))
{
    
}

IcsClient::~IcsClient()
{
    do_error();
}
    
void IcsClient::do_read()
{
	// wait response
	m_socket.async_read_some(asio::buffer(m_recv_buf, sizeof(m_recv_buf)),
		[this](const std::error_code& ec, std::size_t length)
	{
		if (!ec)
		{
			// handle message
			do_handle_msg(m_recv_buf, length);

			// continue to read
			do_read();
		}
		else
		{
            do_error();
		}
	});
}

void IcsClient::do_write()
{
    
}

void IcsClient::do_error()
{
    LOG_DEBUG(m_conn_name << " close the connection");
}

void IcsClient::do_handle_msg(uint8_t* buf, size_t length)
{
    buf[length] = 0;
    LOG_DEBUG("recv:"<<buf);
//	IcsProtocol proto(buf, length);
//	IcsProtocol::IcsMsgHead* mh = proto.getHead();
}


//----------------------------------------------------------------------------------//



}// end namespace ics
