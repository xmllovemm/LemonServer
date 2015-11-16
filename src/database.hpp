//
//  database.hpp
//  ics-server
//
//  Created by lemon on 15/10/21.
//  Copyright 2015年 personal. All rights reserved.
//

#ifndef database_hpp
#define database_hpp

#include "config.hpp"
#include "otlv4.h"
#include <string>
#include <exception>

namespace ics {
    

class DataBase {
public:
//	using OtlConnectPool = otl_connect_pool<otl_connect, otl_exception>;

	typedef otl_connect_pool<otl_connect,otl_exception> OtlConnectPool;

	typedef OtlConnectPool::connect_ptr OtlConnect;

	DataBase(const std::string& uid, const std::string& pwd, const std::string& dsn);
    
	DataBase();

	void init(const std::string& uid, const std::string& pwd, const std::string& dsn);

	void open(int pool_min_size = 8, int pool_max_size = 16) throw(std::runtime_error);

    ~DataBase();
    
    static void initialize(bool multi_thread = true);
    
	OtlConnect getConnection();
    
	void putConnection(OtlConnect conn);
    
private:

	OtlConnectPool	m_conn_pool;

    std::string		m_conn_str;
    
};

class OtlConnectionGuard {
public:
	OtlConnectionGuard(DataBase& db) :m_db(db), m_connection(m_db.getConnection())
	{

	}

	~OtlConnectionGuard()
	{
		m_db.putConnection(std::move(m_connection));
	}

	otl_connect& connection()
	{
		return *m_connection;
	}

private:
	DataBase&	m_db;
	DataBase::OtlConnect m_connection;
};

}   // namespace ics

#endif /* database_hpp */
