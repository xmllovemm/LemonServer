
#include "config.h"
#include "tcpclient.h"
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


IcsSimulateClient::IcsSimulateClient(IcsSimulateClient&& rhs)
	: m_socket(std::move(rhs.m_socket)), m_authrize_info(rhs.m_authrize_info)
{

}


//*/

void IcsSimulateClient::start(asio::ip::tcp::resolver::iterator endpoint_iter)
{
	asio::async_connect(m_socket, endpoint_iter,
		[this](std::error_code ec, asio::ip::tcp::resolver::iterator it)
	{
		if (!ec)
		{
			do_authrize();

			// start read data
			do_read();
		}
		else
		{
			LOG_DEBUG("Connect to server failed, as: " << ec.message());
		}
	});
}

void IcsSimulateClient::do_authrize()
{
	// prepare ics protocol data
	char buf[126];
	IcsProtocol protocol(buf, sizeof(buf));
	protocol.initHead(IcsProtocol::IcsMsg_auth_request, m_send_num++);
	protocol << m_authrize_info.m_gw_id << m_authrize_info.m_gw_pwd << m_authrize_info.m_device_kind << m_authrize_info.m_ext_info;
	protocol.serailzeToData();

	// send
	asio::async_write(m_socket, asio::buffer(protocol.addr(), protocol.length()),
		[this](const std::error_code& ec, std::size_t length)
	{
		if (!ec)
		{
			LOG_DEBUG("Send auth success!");
		}
		else
		{
			m_socket.close();
		}
	});
}

void IcsSimulateClient::do_read()
{
	// wait response
	asio::async_read(m_socket, asio::buffer(m_recv_buf, sizeof(m_recv_buf)),
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
			m_socket.close();
			LOG_DEBUG("Close the connection: " << ec.message());
		}
	});
}

void IcsSimulateClient::do_write()
{

}

void IcsSimulateClient::do_close()
{
	//m_io_service.post([this](){ m_socket.close(); });
}

void IcsSimulateClient::do_handle_msg(uint8_t* buf, size_t length)
{
	IcsProtocol proto(buf, length);
	IcsProtocol::IcsMsgHead* mh = proto.getHead();
}


//----------------------------------------------------------------------------------//



}// end namespace ics
