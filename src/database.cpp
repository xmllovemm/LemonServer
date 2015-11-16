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
	init(uid, pwd, dsn);
}
 
DataBase::DataBase()
{

}

DataBase::~DataBase()
{

}

void DataBase::init(const std::string& uid, const std::string& pwd, const std::string& dsn)
{
	m_conn_str = uid + "/" + pwd + "@" + dsn;
}

void DataBase::open(int pool_min_size, int pool_max_size) throw(std::runtime_error)
{
	if (m_conn_str.empty())
	{
		throw std::runtime_error("connection string is empty");
	}
	m_conn_pool.open(m_conn_str.c_str(), false, pool_min_size, pool_max_size);
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