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


class TcpConnection {
public:
	TcpConnection(asio::ip::tcp::socket&& s)
		: m_socket(std::forward<asio::ip::tcp::socket>(s))
	{
		
	}
        
	virtual ~TcpConnection()
	{

	}
    
	virtual void start() = 0;

protected:
	void do_error()
	{
		m_socket.close();
		delete this;
	}
        
protected:
    asio::ip::tcp::socket   m_socket;
};
    
}

#endif // _TCP_CONNECTION_H
