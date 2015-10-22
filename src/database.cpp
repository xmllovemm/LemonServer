//
//  database.cpp
//  ics-server
//
//  Created by lemon on 15/10/21.
//  Copyright 2015年 personal. All rights reserved.
//

#include "database.hpp"
#include "log.hpp"

namespace ics {
DataBase::DataBase(const std::string& uid, const std::string& pwd, const std::string& dsn)
{
    
}
    
DataBase::~DataBase()
{
    
}
    
void DataBase::initialize()
{
 //   otl_connect::otl_initialize();
}
    
otl_connect* DataBase::getConnection()
{
	otl_connect* conn = nullptr;
    try {
        conn = new otl_connect();
        conn->rlogon(m_conn_str.c_str());
    }
    catch (otl_exception& ex) {
        LOG_DEBUG("connect to "<<m_conn_str<<" failed:"<<ex.msg);
    }
	return conn;
}
    
void DataBase::putConnection(otl_connect* conn)
{
        
}

}