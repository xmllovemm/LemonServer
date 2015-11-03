

#include "mempool.hpp"

namespace ics {

MemoryChunk::MemoryChunk(void* buf, std::size_t length)
	: m_buff(buf), m_usedSize(0), m_totalSize(length)
{

}

MemoryChunk::MemoryChunk(const MemoryChunk& rhs)
	: m_buff(rhs.m_buff), m_usedSize(rhs.m_usedSize), m_totalSize(rhs.m_totalSize)
{

}

MemoryChunk::MemoryChunk()
	: m_buff(NULL), m_usedSize(0), m_totalSize(0)
{

}

MemoryChunk& MemoryChunk::operator = (const MemoryChunk& rhs)
{
	m_buff = rhs.m_buff;
	m_usedSize = rhs.m_usedSize;
	m_totalSize = rhs.m_totalSize;
	return *this;
}

void* MemoryChunk::getBuff()
{
	return m_buff;
}

std::size_t MemoryChunk::getUsedSize()
{
	return m_usedSize;
}

void MemoryChunk::setUsedSize(std::size_t n)
{
	m_usedSize = n;
}

std::size_t MemoryChunk::getTotalSize()
{
	return m_totalSize;
}

bool MemoryChunk::valid()
{
	return m_buff != NULL;
}



MemoryPool::MemoryPool(std::size_t totalSize, std::size_t countOfChunk, bool zeroData)
	: m_buff(new char[totalSize]), m_count(countOfChunk)
{
	if (zeroData)
	{
		std::memset(m_buff, 0, totalSize);
	}
	std::size_t chunkSize = totalSize / countOfChunk;
	for (size_t i = 0; i < countOfChunk; i++)
	{
		MemoryChunk mc(m_buff + i*chunkSize, chunkSize);
		m_chunkList.push_back(mc);
//		m_chunkList.push_back(MemoryChunk{ (char*)m_buff + i*chunkSize, chunkSize });
	}
}

MemoryPool::~MemoryPool()
{
	delete[] m_buff;
}

MemoryChunk MemoryPool::get()
{
	std::lock_guard<std::mutex> lock(m_chunkLock);
	MemoryChunk chunk;
	if (!m_chunkList.empty())
	{
		chunk = m_chunkList.front();
	}
	return chunk;
}

void MemoryPool::put(MemoryChunk& chunk)
{
	std::lock_guard<std::mutex> lock(m_chunkLock);
	m_chunkList.push_back(chunk);
}

}