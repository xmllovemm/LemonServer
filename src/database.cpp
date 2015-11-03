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
	//:m_conn_str("UID=" + uid + ";PWD=" + pwd + ";DSN=" + dsn)
	: m_conn_str(uid + "/" + pwd + "@" + dsn)
{
	
}
    
DataBase::~DataBase()
{

}

void DataBase::open(int pool_min_size, int pool_max_size) throw(std::runtime_error)
{
	try {
		m_conn_pool.open(m_conn_str.c_str(), false, pool_min_size, pool_max_size);
	}
	catch (otl_exception& ex)
	{
		LOG_ERROR("open connect pool error:" << ex.msg);
	}
}

void DataBase::initialize(bool multi_thread)
{
	otl_connect::otl_initialize(multi_thread);
}
    
DataBase::OtlConnect DataBase::getConnection()
{
	return m_conn_pool.get();
}
    
void DataBase::putConnection(DataBase::OtlConnect conn)
{
	m_conn_pool.put(std::move(conn));
}

}