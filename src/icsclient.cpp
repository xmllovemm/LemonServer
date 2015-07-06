
#include "config.h"
#include "icsclient.h"
#include "icsprotocol.h"


namespace ics{

//----------------------------------------------------------------------------------//

AuthrizeInfo::AuthrizeInfo(const AuthrizeInfo& rhs)
{
	m_gw_id = rhs.m_gw_id;
	m_gw_pwd = rhs.m_gw_pwd;
	m_ext_info = rhs.m_ext_info;
}

AuthrizeInfo::AuthrizeInfo(AuthrizeInfo&& rhs)
{
	m_gw_id = rhs.m_gw_id;
	m_gw_pwd = rhs.m_gw_pwd;
	m_ext_info = rhs.m_ext_info;
}

AuthrizeInfo::AuthrizeInfo(const string& gw_id, const string& gw_pwd, uint16_t device_kind, const string& ext_info)
	: m_gw_id(gw_id), m_gw_pwd(gw_pwd), m_device_kind(device_kind), m_ext_info(ext_info)
{

}


AuthrizeInfo::AuthrizeInfo(string&& gw_id, string&& gw_pwd, uint16_t device_kind, string&& ext_info)
: m_gw_id(std::move(gw_id)), m_gw_pwd(std::move(gw_pwd)), m_device_kind(device_kind), m_ext_info(std::move(ext_info))
{
	
}

/*
AuthrizeInfo::AuthrizeInfo(const char* gw_id, const char* gw_pwd, uint16_t device_kind, const char* ext_info)
	: m_gw_id(gw_id), m_gw_pwd(gw_pwd), m_device_kind(device_kind), m_ext_info(ext_info)
{

}
*/
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


IcsSimulateClient::IcsSimulateClient(asio::ip::tcp::socket&& socket, AuthrizeInfo&& info)
	: m_socket(std::move(socket)), m_authrize_info(std::move(info))
{
	LOG_DEBUG("call IcsSimulateClient rvalue reference");
}

IcsSimulateClient::IcsSimulateClient(asio::ip::tcp::socket&& s, const AuthrizeInfo& info)
	: m_socket(std::move(s)), m_authrize_info(info)
{
	LOG_DEBUG("call IcsSimulateClient const lvalue reference");
}

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
	protocol << m_authrize_info.m_gw_id << m_authrize_info.m_gw_pwd  << m_authrize_info.m_device_kind << m_authrize_info.m_ext_info;
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
//	m_io_service.post([this](){ m_socket.close(); });
}

void IcsSimulateClient::do_handle_msg(uint8_t* buf, size_t length)
{
	IcsProtocol proto(buf, length);
	IcsProtocol::IcsMsgHead* mh = proto.getHead();
}


//----------------------------------------------------------------------------------//


IcsClientManager::IcsClientManager(const string& ip, const string& port)
{
	asio::ip::tcp::resolver resolver(m_io_service);
	m_server_endpoint = resolver.resolve({ ip, port });
}

IcsClientManager::~IcsClientManager()
{
	LOG_DEBUG("call ~IcsClientManager()");
	m_io_service.stop();
}

void IcsClientManager::run()
{
	m_thread.reset(new std::thread(
		[this]()
		{
			LOG_DEBUG("Thread start...");
			while (true)
			{
				m_io_service.run();
				LOG_DEBUG("after io server run,sleep for 3s...");
				std::this_thread::sleep_for(std::chrono::seconds(3));
			}
			LOG_DEBUG("Thread exit...");
		}));
}

void IcsClientManager::stop()
{
	m_io_service.stop();
}

void IcsClientManager::createIcsClient(AuthrizeInfo&& info)
{
	LOG_DEBUG("call createIcsClient rvalue reference");

	auto c = std::make_shared<IcsSimulateClient>(asio::ip::tcp::socket(m_io_service), std::move(info));
	c->start(m_server_endpoint);
}

void IcsClientManager::createIcsClient(const AuthrizeInfo& info)
{
	LOG_DEBUG("call createIcsClient const lvalue reference");

	auto c = std::make_shared<IcsSimulateClient>(asio::ip::tcp::socket(m_io_service), info);
	c->start(m_server_endpoint);
}

}	// end namespace ics
