

#ifndef _MEMORY_POOL_H
#define _MEMORY_POOL_H

//#include "icsconfig.hpp"
#include <mutex>
#include <list>
#include <memory>

namespace ics {

class MemoryPool;

class MemoryChunk 
{
public:
	MemoryChunk();

	MemoryChunk(void* buf, std::size_t usedLength);

	MemoryChunk(const MemoryChunk& rhs);

	MemoryChunk(MemoryChunk&& rhs);

	~MemoryChunk();

	MemoryChunk clone(MemoryPool& mp);

	void operator = (MemoryChunk&& rhs);

	MemoryChunk& operator = (const MemoryChunk& rhs);

	bool valid() const;
public:
	// 地址
	uint8_t*	data;
	// 长度
	std::size_t	length;

	// 可用最大长度
//	std::size_t	size;
};


class MemoryPool {
public:
	MemoryPool(std::size_t chunkSize, std::size_t countOfChunk, bool zeroData = true);

	MemoryPool();

	void init(std::size_t chunkSize, std::size_t countOfChunk, bool zeroData = true);

	~MemoryPool();

	MemoryChunk get();

	void put(const MemoryChunk& chunk);

	std::size_t chunkSize() const;
private:
	uint8_t*		m_buff;
	std::size_t		m_chunkSize;
	std::size_t		m_chunkCount;

	std::list<uint8_t*>	m_chunkList;
	std::mutex		m_chunkLock;
};

}

#endif	// _MEMORY_POOL_H