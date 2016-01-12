

#include "downloadfile.hpp"
#include "database.hpp"
#include "log.hpp"
#include "icsexception.hpp"
#include <cstdio>
#include <stdio.h>
#include <fcntl.h>

#ifdef WIN32
#include <Windows.h>
#else
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif


extern ics::DataBase g_database;


namespace ics {


FileUpgradeManager::FileInfo::FileInfo(const std::string& filename)
	: file_name(filename)
{
#ifndef WIN32
	// 打开文件
	int fd = open(this->file_name.c_str(), O_RDONLY);
	if (fd < 0)
	{
		throw IcsException("open file %s failed,as %s", this->file_name.c_str(), strerror(errno));
	}

	// 获取文件大小
	this->file_length = lseek(fd, 0, SEEK_END);

	// 文件大小不能超过最大限制
	if (this->file_length > UPGRADEFILE_MAXSIZE)
	{
		close(fd);
		throw IcsException("file %s 's size=%d bytes it too big", this->file_name.c_str(), this->file_length);
	}

	// 文件内容映射到内存
	this->file_content = mmap(NULL, this->file_length, PROT_READ, MAP_PRIVATE, fd, 0);
	if (this->file_content == MAP_FAILED)
	{
		close(fd);
		throw IcsException("mmap file %s failed,as %s", this->file_name.c_str(), strerror(errno));
	}
#else
	/// open file read only
	HANDLE hFile = CreateFile(this->file_name.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

	if (!hFile || hFile == INVALID_HANDLE_VALUE)
	{
		throw IcsException("CreateFile %s failed,ErrorNum:%d", this->file_name.c_str(),GetLastError());
	}
	this->file_length = GetFileSize(hFile, NULL);

	/// 创建一个文件映射内核对象
	HANDLE hFileMap = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
	CloseHandle(hFile);
	if (hFileMap == INVALID_HANDLE_VALUE)
	{
		throw IcsException("CreateFileMapping %s failed", this->file_name.c_str());
	}

	/// 把文件数据映射到进程的地址空间
	this->file_content = MapViewOfFile(hFileMap, FILE_MAP_READ, 0, 0, 0);
	if (!this->file_content)
	{
		CloseHandle(hFileMap);
		throw IcsException("MapViewOfFile %s failed", this->file_name.c_str());
	}
#endif
}

FileUpgradeManager::FileInfo::~FileInfo()
{
	if (this->file_content)
	{
#ifndef WIN32
		munmap(this->file_content, this->file_length);
#else
		UnmapViewOfFile(this->file_content);
#endif
	}
}



FileUpgradeManager* FileUpgradeManager::s_instance = NULL;

FileUpgradeManager::FileUpgradeManager()
{
}

FileUpgradeManager::~FileUpgradeManager()
{
}

FileUpgradeManager* FileUpgradeManager::getInstance()
{
	if (!s_instance)
	{
		s_instance = new FileUpgradeManager();
	}

	return s_instance;
}

/// 根据文件ID查找文件信息
std::shared_ptr<FileUpgradeManager::FileInfo> FileUpgradeManager::getFileInfo(uint32_t fileid) throw()
{
	auto it = m_fileMap.find(fileid);
	if (it != m_fileMap.end())	// 已找到
	{
		return it->second;
	}
	else	// 未找到,尝试加载
	{
#ifdef ICS_CENTER_MODE	
		try {
			// ICS中心模式可从数据库中尝试读取文件
			auto fileinfo = loadFileInfo(fileid);
			m_fileMap[fileid] = fileinfo;
			return fileinfo;
		}
		catch (IcsException& ex)
		{
			LOG_ERROR("FileUpgradeManager get file error:" << ex.message());
		}
		catch (std::exception& ex)
		{
			LOG_ERROR("FileUpgradeManager get file error:" << ex.what());
		}
		catch (otl_exception& ex)
		{
			LOG_ERROR("FileUpgradeManager get file error:" << ex.msg);
		}
#else
		// 代理模式无法查询到文件路径
		LOG_ERROR("FileUpgradeManager can't get info file by fileid");
#endif
		return nullptr;
	}
}

/// 根据就文件ID从数据库查询文件名后加载文件信息
std::shared_ptr<FileUpgradeManager::FileInfo> FileUpgradeManager::loadFileInfo(uint32_t fileid) throw(IcsException, otl_exception)
{
	// 根据文件id从数据库读取文件路径
	OtlConnectionGuard connGuard(g_database);
	otl_stream s(1, "{ call sp_upgrade_getfile(:fileid<int,in>,@filename) }", connGuard.connection());
	s << (int)fileid;

	otl_stream queryResult(1, "select @filename :#filename<char[126]>", connGuard.connection());
	std::string filename;
	queryResult >> filename;

	if (filename.empty())
	{
		LOG_ERROR("cann't find upgrade file by fileid: " << fileid);
	}

	return loadFileInfo(fileid, filename);
}

/// 根据文件ID及其对应文件名创建文件信息
std::shared_ptr<FileUpgradeManager::FileInfo> FileUpgradeManager::loadFileInfo(uint32_t fileid, const std::string& filename) throw(IcsException)
{
	std::lock_guard<std::mutex> lock(m_loadFileLock);
	// 尝试查找文件id对应内容
	auto& it = m_fileMap[fileid];
	if (!it)
	{
		it = std::make_shared<FileInfo>(filename);
	}
	return it;
}

}
