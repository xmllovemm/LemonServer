//
//  tcpconnection.hpp
//  ics-server
//
//  Created by lemon on 15/10/22.
//  Copyright © 2015年 personal. All rights reserved.
//

#ifndef _TCP_CONNECTION_H
#define _TCP_CONNECTION_H


#include "config.hpp"


namespace ics {

typedef asio::ip::tcp icstcp;
typedef asio::ip::udp icsudp;

template<class ProtocolType = icstcp>
class Connection {
public:
	typedef typename asio::basic_stream_socket<ProtocolType> socket;

	Connection(socket&& s)
		: m_socket(std::forward<socket>(s)), m_replaced(false)
	{

	}

	virtual ~Connection()
	{
		m_socket.close();
	}

	virtual void start() = 0;

	void replaced(bool flag = true)
	{
		m_replaced = flag;
	}

protected:
	// tcp socket
	socket	m_socket;

	// replaced by another TcpConnection
	bool	m_replaced;
};
    
}

#endif // _TCP_CONNECTION_H
