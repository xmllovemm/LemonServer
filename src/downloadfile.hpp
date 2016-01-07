

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


// �ļ�����
class FileUpgradeManager
{
public:

	// �����ļ���Ϣ
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

	/// �����ļ�ID�����ļ���Ϣ
	std::shared_ptr<FileInfo> getFileInfo(uint32_t fileid) throw();

	/// �����ļ�ID�����Ӧ�ļ��������ļ���Ϣ
	std::shared_ptr<FileInfo> loadFileInfo(uint32_t fileid, const std::string& filename) throw(IcsException);

public:
	static FileUpgradeManager* getInstance();

private:
	/// ���ݾ��ļ�ID�����ݿ��ѯ�ļ���������ļ���Ϣ
	std::shared_ptr<FileInfo> loadFileInfo(uint32_t fileid) throw(IcsException, otl_exception);

private:
	// �ļ�idӳ���
	std::unordered_map<uint32_t, std::shared_ptr<FileInfo>> m_fileMap;

	// �����ļ�������
	std::mutex		m_loadFileLock;

private:
	static FileUpgradeManager* s_instance;
};

}

#endif // _DOWN_LOAD_FILE_H
