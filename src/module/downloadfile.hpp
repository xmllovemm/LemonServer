

#ifndef _DOWN_LOAD_FILE_H
#define _DOWN_LOAD_FILE_H

#include "icsexception.hpp"
#include "config.hpp"
#include "otlv4.h"
#include <memory>
#include <unordered_map>
#include <mutex>
#include <string>


namespace ics {

#define UPGRADEFILE_MAXSIZE 1024*1024*50	// 50MB
#define UPGRADE_FILE_SEGMENG_SIZE 512


// 文件升级
class FileUpgradeManager
{
public:

	// 升级文件信息
	struct FileInfo
	{
		FileInfo(const std::string& filename);

		~FileInfo();
		
		void* file_content;
		uint32_t file_length;
		std::string file_name;
	};

	FileUpgradeManager();

	~FileUpgradeManager();

	/// 根据文件ID查找文件信息
	std::shared_ptr<FileInfo> getFileInfo(uint32_t fileid) throw();

	/// 根据文件ID及其对应文件名创建文件信息
	std::shared_ptr<FileInfo> loadFileInfo(uint32_t fileid, const std::string& filename) throw(IcsException);

public:
	static FileUpgradeManager* getInstance();

private:
	/// 根据就文件ID从数据库查询文件名后加载文件信息
	std::shared_ptr<FileInfo> loadFileInfo(uint32_t fileid) throw(IcsException, otl_exception);

private:
	// 文件id映射表
	std::unordered_map<uint32_t, std::shared_ptr<FileInfo>> m_fileMap;

	// 加载文件互斥锁
	std::mutex		m_loadFileLock;

private:
	static FileUpgradeManager* s_instance;
};

}

#endif // _DOWN_LOAD_FILE_H
