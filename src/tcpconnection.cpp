//
//  tcpconnection.cpp
//  ics-server
//
//  Created by lemon on 15/10/22.
//  Copyright © 2015年 personal. All rights reserved.
//


#include "tcpconnection.hpp"

namespace ics {

TcpConnection::TcpConnection(asio::ip::tcp::socket&& s, ClientManager & cm)
	: m_socket(std::forward<asio::ip::tcp::socket>(s)), m_client_manager(cm)
{}

TcpConnection::~TcpConnection()
{
	m_socket.close();
}

void TcpConnection::do_error()
{
	m_client_manager.removeClient(this);
}

}