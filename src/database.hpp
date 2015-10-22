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

namespace ics {
    
class DataBase {
public:
    DataBase(const std::string& uid, const std::string& pwd, const std::string& dsn);
    
    ~DataBase();
    
    static void initialize();
    
    otl_connect* getConnection();
    
    void putConnection(otl_connect* conn);
    
private:
    std::string m_conn_str;
    
};
    
}   // namespace ics

#endif /* database_hpp */
