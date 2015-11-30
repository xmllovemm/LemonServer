

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

std::shared_ptr<FileUpgradeManager::FileInfo> FileUpgradeManager::getFileInfo(uint32_t fileid)
{
	auto it = m_fileMap.find(fileid);
	if (it != m_fileMap.end())	// ���ҵ�
	{
		return it->second;
	}
	else	// δ�ҵ�,���Լ���
	{
		return loadFileInfo(fileid);
	}
}

std::shared_ptr<FileUpgradeManager::FileInfo> FileUpgradeManager::loadFileInfo(uint32_t fileid)
{
	// ��ȡ������
	std::lock_guard<std::mutex> lock(m_loadFileLock);

	// ���Բ����ļ�id��Ӧ����
	if (m_fileMap.find(fileid) != m_fileMap.end())
	{
		return m_fileMap[fileid];
	}

	// �����µ��ļ���Ϣ
	auto fileInfo = std::make_shared<FileInfo>();

	// �����ļ�id�����ݿ��ȡ�ļ�·��
	{
		OtlConnectionGuard connGuard(g_database);
		otl_stream s(1, "{ sp_upgrade_getfile(:F1<int,in>,@filename) }", connGuard.connection());
		s << (int)fileid;

		otl_stream queryResult(1, "{ sp_upgrade_getfile(:F1<int,in>,@filename) }", connGuard.connection());
		queryResult >> fileInfo->file_name;

		if (fileInfo->file_name.empty())
		{
			LOG_ERROR("cann't find upgrade file by fileid: " << fileid);
		}
	}
#ifndef WIN32
	// ���ļ�
	int fd = open(fileInfo->file_name.c_str(), O_RDONLY);
	if (fd < 0)
	{
		throw IcsException("open file %s failed,as %s", fileInfo->file_name, strerror(errno));
	}

	// ��ȡ�ļ���С
	fileInfo->file_length = lseek(fd, 0, SEEK_END);

	// �ļ���С���ܳ����������
	if (fileInfo->file_length > UPGRADEFILE_MAXSIZE)
	{
		close(fd);
		throw IcsException("file %s 's size it too big", fileInfo->file_name.c_str());
	}

	// �ļ�����ӳ�䵽�ڴ�
	fileInfo->file_content = mmap(NULL, fileInfo->file_length, PROT_READ, MAP_PRIVATE, fd, 0);
	if (fileInfo->file_content == MAP_FAILED)
	{
		close(fd);
		throw IcsException("mmap file %s failed,as %s", fileInfo->file_name.c_str(), strerror(errno));
	}
#else
	/// open file read only
	HANDLE hFile = CreateFile(fileInfo->file_name.c_str(), GENERIC_READ, READ_CONTROL, 0, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		throw IcsException("CreateFile %s failed", fileInfo->file_name.c_str());
	}

	/// ����һ���ļ�ӳ���ں˶���
	HANDLE hFileMap = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
	CloseHandle(hFile);
	if (hFileMap == INVALID_HANDLE_VALUE)
	{
		throw IcsException("CreateFileMapping %s failed", fileInfo->file_name.c_str());
	}

	/// ���ļ�����ӳ�䵽���̵ĵ�ַ�ռ�
	fileInfo->file_content = MapViewOfFile(hFileMap, FILE_MAP_READ, 0, 0, 0);
	if (!fileInfo->file_content)
	{
		CloseHandle(hFileMap);
		throw IcsException("");
	}
#endif

	// �µ��ļ���Ϣ���뵽�ļ�ӳ���
	m_fileMap[fileid] = fileInfo;

	// �ɹ�����
	return fileInfo;
}

}
