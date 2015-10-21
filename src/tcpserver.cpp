

#include "tcpserver.h"
#include "log.h"


namespace ics{

TcpServer::TcpServer(const std::string& ip, int port, std::function<void (asio::ip::tcp::socket)> do_add_client)
	: m_client_socket(m_io_service)
	, m_acceptor(m_io_service, asio::ip::tcp::endpoint(asio::ip::address::from_string(ip), port))
	, m_do_add_client(do_add_client)
{
	LOG_DEBUG("Tcp server starts to listen at " << ip << ":" << port);
}


TcpServer::TcpServer(int port, std::function<void(asio::ip::tcp::socket)> do_add_client)
	: m_client_socket(m_io_service)
	, m_acceptor(m_io_service, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))
	, m_do_add_client(do_add_client)
{
	LOG_DEBUG("Tcp server starts to listen at localhost:" << port);
}

TcpServer::~TcpServer()
{
	
}

void TcpServer::run()
{
	LOG_DEBUG("Tcp server starts to accept client");
	do_accept();
	m_io_service.run();
}

void TcpServer::run_on_thread()
{
	if (m_io_service_thread != nullptr)
	{
		delete m_io_service_thread;
	}
	m_io_service_thread = new std::thread([this](){
									run();
								});
}

void TcpServer::stop()
{
	LOG_DEBUG("Tcp server stops");
	m_io_service.stop();
}

void TcpServer::do_accept()
{
	// accept client
	m_acceptor.async_accept(m_client_socket,
		[this](std::error_code ec)
		{
			if (!ec)
			{
				m_do_add_client(std::move(m_client_socket));
			}

			// accept next client
			do_accept();
		});
}

} // end namespace ics
