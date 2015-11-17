

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

MemoryChunk MemoryChunk::clone(MemoryPool& mp)
{
	MemoryChunk mc = mp.get();
	mc.m_usedSize = m_usedSize > mc.m_totalSize ? mc.m_totalSize : m_usedSize;
	std::memcpy(mc.m_buff, m_buff, mc.m_usedSize);
	return mc;
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



MemoryPool::MemoryPool(std::size_t chunkSize, std::size_t countOfChunk, bool zeroData)
{
	init(chunkSize, countOfChunk, zeroData);
}

MemoryPool::MemoryPool()
{

}

MemoryPool::~MemoryPool()
{
	delete[] m_buff;
}

void MemoryPool::init(std::size_t chunkSize, std::size_t countOfChunk, bool zeroData)
{
	m_chunkSize = chunkSize;
	m_chunkCount = countOfChunk;
	m_buff = new char[m_chunkSize * m_chunkCount];

	if (zeroData)
	{
		std::memset(m_buff, 0, m_chunkSize * m_chunkCount);
	}

	for (size_t i = 0; i < m_chunkCount; i++)
	{
		MemoryChunk mc(m_buff + i*m_chunkSize, m_chunkSize);
		m_chunkList.push_back(mc);
	}
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

void MemoryPool::put(MemoryChunk&& chunk)
{
	std::lock_guard<std::mutex> lock(m_chunkLock);
	m_chunkList.push_back(chunk);
}

void MemoryPool::put(MemoryChunk& chunk)
{
	put(std::move(chunk));
}

}