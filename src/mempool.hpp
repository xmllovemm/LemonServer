

#ifndef _MEMORY_POOL_H
#define _MEMORY_POOL_H

//#include "config.hpp"
#include <mutex>
#include <list>
#include <memory>

namespace ics {

class MemoryPool;

class MemoryChunk {
public:
	MemoryChunk() = delete;

	MemoryChunk(MemoryPool& mp);

	~MemoryChunk();

	MemoryChunk(MemoryChunk&& rhs);

	MemoryChunk clone(MemoryPool& mp);

	MemoryChunk& operator = (MemoryChunk&& rhs);

	void* getBuff();

	// get used size
	std::size_t getUsedSize() const;

	void setUsedSize(std::size_t n);

	// get tatol size
	std::size_t getTotalSize() const;

	bool valid() const;

public:
	MemoryPool&	m_memPool;
	uint8_t*		m_buff;
	std::size_t	m_usedSize;
	std::size_t	m_totalSize;
};

typedef std::unique_ptr<MemoryChunk> MemoryChunk_ptr;

class MemoryPool {
public:
	MemoryPool(std::size_t chunkSize, std::size_t countOfChunk, bool zeroData = true);

	MemoryPool();

	void init(std::size_t chunkSize, std::size_t countOfChunk, bool zeroData = true);

	~MemoryPool();

	MemoryChunk_ptr get();

	void put(MemoryChunk_ptr&& chunk);

	std::size_t chunkSize() const;
private:
	char*		m_buff;
	std::size_t	m_chunkSize;
	std::size_t	m_chunkCount;

	std::list<MemoryChunk_ptr>	m_chunkList;
	std::mutex		m_chunkLock;
};

}

#endif	// _MEMORY_POOL_H