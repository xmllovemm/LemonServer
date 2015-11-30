

#ifndef _DOWN_LOAD_FILE_H
#define _DOWN_LOAD_FILE_H

#include <memory>
#include <unordered_map>
#include <mutex>
#include <string>

namespace ics {

#define UPGRADEFILE_MAXSIZE 102400
// �ļ�����
class FileUpgradeManager
{
public:

	// �����ļ���Ϣ
	struct FileInfo
	{
		void* file_content;
		uint32_t file_length;
		std::string file_name;
	};

	FileUpgradeManager();

	~FileUpgradeManager();

	std::shared_ptr<FileInfo> getFileInfo(uint32_t fileid);

public:
	static FileUpgradeManager* getInstance();

private:
	std::shared_ptr<FileInfo> loadFileInfo(uint32_t fileid);

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
