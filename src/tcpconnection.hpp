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
#include "clientmanager.hpp"


namespace ics {

class ClientManager;

class TcpConnection {
public:
	TcpConnection(asio::ip::tcp::socket&& s, ClientManager & cm);
        
	virtual ~TcpConnection();
        
    virtual void do_read() = 0;
        
    virtual void do_write() = 0;
    
protected:
	void do_error();
        
protected:
    asio::ip::tcp::socket   m_socket;
    std::string             m_conn_name;
    ClientManager&			m_client_manager;
};
    
}

#endif // _TCP_CONNECTION_H
