/*
ics server
*/



#ifndef _ICS_SERVER_H
#define _ICS_SERVER_H

#include "config.h"
#include <asio.hpp>

namespace ics{

	class TcpConnection{
	public:
		TcpConnection(asio::ip::tcp::socket s):m_socket(std::move(s)){}
		
		~TcpConnection(){}

	protected:
		asio::ip::tcp::socket m_socket;
	};


	class TcpServer
	{
	public:

		TcpServer() = delete;

		TcpServer(asio::ip::tcp::endpoint endpoint);

		void run();

		void stop();
	private:
		void do_accept();

	private:
		asio::io_service		m_io_service;
		asio::ip::tcp::acceptor	m_acceptor;
		asio::ip::tcp::socket	m_socket;
	};

} // end namespace ics
#endif	// end _ICS_SERVER_H
