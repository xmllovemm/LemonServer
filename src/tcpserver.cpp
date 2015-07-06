

#include "tcpserver.h"

namespace ics{


	TcpServer::TcpServer(asio::ip::tcp::endpoint endpoint) :m_acceptor(m_io_service, endpoint), m_socket(m_io_service)
	{
		// start accept the client
		do_accept();
	}

	void TcpServer::do_accept()
	{
		m_acceptor.async_accept(m_socket,
			[this](std::error_code ec)
			{
				if (!ec)
				{

					
				}
				do_accept();
			});
	}

}	// end namespace ics
