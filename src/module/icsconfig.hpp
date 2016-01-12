

#ifndef _ICS_CONFIG_H
#define _ICS_CONFIG_H

#include "config.hpp"
#include "tinyxml.h"
#include "icsexception.hpp"
#include <string>
#include <unordered_map>


namespace ics {

class IcsConfig {
public:
	IcsConfig();

	~IcsConfig();

	void load(const char* file) throw(IcsException);

	void reload(const char* file) throw(IcsException);

	const std::string& getAttributeString(const char* module, const char* key) throw(IcsException);

	int getAttributeInt(const char* module, const char* key) throw(IcsException);

private:
	std::unordered_map<std::string, std::unordered_map<std::string, std::string>> m_attributeMap;
};

}

#endif	// _ICS_CONFIG_HPP