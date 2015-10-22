//
//  tcpconnection.hpp
//  ics-server
//
//  Created by lemon on 15/10/22.
//  Copyright © 2015年 personal. All rights reserved.
//

#ifndef tcpconnection_hpp
#define tcpconnection_hpp

//
//  tcpconnection.cpp
//  ics-server
//
//  Created by lemon on 15/10/22.
//  Copyright © 2015年 personal. All rights reserved.
//

#include "config.h"
#include "clientmanager.h"


namespace ics {
    
    class TcpConnection {
    public:
        TcpConnection(asio::ip::tcp::socket s, ClientManager & cm)
        : m_socket(std::move(s)),m_client_manager(cm)
        {}
        
        virtual ~TcpConnection()
        {
            m_socket.close();
        }
        
        virtual void do_read() = 0;
        
        virtual void do_write() = 0;
        
        void do_error()
        {
            m_client_manager.removeClient(this);
        }
        
    protected:
        asio::ip::tcp::socket   m_socket;
        std::string             m_conn_name;
        const ClientManager&    m_client_manager;
    };
    
}

#endif /* tcpconnection_hpp */
