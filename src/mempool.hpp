

#ifndef _MEMORY_POOL_H
#define _MEMORY_POOL_H

#include "config.hpp"
#include <mutex>
#include <list>

namespace ics {

class MemoryPool;

class MemoryChunk {
public:
	MemoryChunk(void* buf, std::size_t length);

	MemoryChunk();

	MemoryChunk(const MemoryChunk& rhs);

	MemoryChunk clone(MemoryPool& mp);

	MemoryChunk& operator = (const MemoryChunk& rhs);

	void* getBuff();

	// get used size
	std::size_t getUsedSize();

	void setUsedSize(std::size_t n);

	// get tatol size
	std::size_t getTotalSize();

	bool valid();

private:
	void*		m_buff;
	std::size_t	m_usedSize;
	std::size_t	m_totalSize;
};

class MemoryPool {
public:
	MemoryPool(std::size_t chunkSize, std::size_t countOfChunk, bool zeroData = true);

	MemoryPool();

	void init(std::size_t chunkSize, std::size_t countOfChunk, bool zeroData = true);

	~MemoryPool();

	MemoryChunk get();

	void put(MemoryChunk&& chunk);

	void put(MemoryChunk& chunk);
private:
	char*		m_buff;
	std::size_t	m_chunkSize;
	std::size_t	m_chunkCount;

	std::list<MemoryChunk>	m_chunkList;
	std::mutex		m_chunkLock;
};

}

#endif	// _MEMORY_POOL_H