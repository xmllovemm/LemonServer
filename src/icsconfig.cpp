

#include "icsconfig.hpp"

namespace ics {

IcsConfig::IcsConfig()
{

}

IcsConfig::~IcsConfig()
{

}

void IcsConfig::load(const char* file) throw(IcsException)
{
	reload(file);
}

void IcsConfig::reload(const char* file) throw(IcsException)
{
	TiXmlDocument doc(file);
	if (!doc.LoadFile())
	{
		throw IcsException("load xml file %s failed as: %s", file, doc.ErrorDesc());
	}

	TiXmlElement* root = doc.RootElement();

	if (root == nullptr)
	{
		doc.Clear();
		throw IcsException("The config file %s don't has root element", file);
	}

	for (TiXmlElement* section = root->FirstChildElement(); section != nullptr; section = section->NextSiblingElement())
	{
		std::unordered_map<std::string, std::string> sectionMap;		
		for (TiXmlElement* node = section->FirstChildElement(); node != nullptr; node = node->NextSiblingElement())
		{
			sectionMap[node->Value()] = node->GetText();
		}
		m_attributeMap[section->Value()] = std::move(sectionMap);
	}
	doc.Clear();
}

const std::string& IcsConfig::getAttributeString(const char* module, const char* key) throw(IcsException)
{
	if (module == nullptr || key == nullptr)
	{
		throw IcsException("cann't be nullptr: module=%s or key=%s", module, key);
	}
	return m_attributeMap[module][key];
}

int IcsConfig::getAttributeInt(const char* module, const char* key) throw(IcsException)
{
	const std::string& val = getAttributeString(module, key);
	return std::strtol(val.c_str(), nullptr, 10);
}

}