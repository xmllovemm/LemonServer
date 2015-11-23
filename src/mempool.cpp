

#include "mempool.hpp"
#include <cstring>

namespace ics {

MemoryChunk::MemoryChunk(MemoryPool& mp)
	: m_memPool(mp)
//	, m_buff(buf), m_usedSize(0), m_totalSize(length)
{
	m_totalSize = mp.chunkSize();
	m_buff = new uint8_t[m_totalSize];
	memset(m_buff, 0, m_totalSize);
	m_usedSize = 0;
}

MemoryChunk::MemoryChunk(MemoryChunk&& rhs)
	: m_memPool(rhs.m_memPool), m_buff(rhs.m_buff), m_usedSize(rhs.m_usedSize), m_totalSize(rhs.m_totalSize)
{
	rhs.m_buff = nullptr;
}


MemoryChunk::~MemoryChunk()
{
	if (m_buff)
	{
		delete (uint8_t*)m_buff;
		m_buff = nullptr;
	}
}

MemoryChunk MemoryChunk::clone(MemoryPool& mp)
{
	/*
	auto mc = mp.get();
	mc->m_usedSize = m_usedSize > mc->m_totalSize ? mc->m_totalSize : m_usedSize;
	std::memcpy(mc->m_buff, m_buff, mc->m_usedSize);
	*/
	MemoryChunk mc(mp);
	memcpy(mc.m_buff, m_buff, m_usedSize);
	return std::move(mc);
}

MemoryChunk& MemoryChunk::operator = (MemoryChunk&& rhs)
{
	m_buff = rhs.m_buff;
	m_usedSize = rhs.m_usedSize;
	m_totalSize = rhs.m_totalSize;
	rhs.m_buff = nullptr;
	return *this;
}

void* MemoryChunk::getBuff()
{
	return m_buff;
}

std::size_t MemoryChunk::getUsedSize() const
{
	return m_usedSize;
}

void MemoryChunk::setUsedSize(std::size_t n)
{
	m_usedSize = n;
}

std::size_t MemoryChunk::getTotalSize() const
{
	return m_totalSize;
}

bool MemoryChunk::valid() const
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
//		MemoryChunk_ptr mc(new MemoryChunk(m_buff + i*m_chunkSize, m_chunkSize));
//		m_chunkList.push_back(std::move(mc));
//		m_chunkList.push_back(std::make_unique<MemoryChunk>(m_buff + i*m_chunkSize, m_chunkSize));	// since c++ 14
	}
}

MemoryChunk_ptr MemoryPool::get()
{
	std::lock_guard<std::mutex> lock(m_chunkLock);
	MemoryChunk_ptr chunk;
	if (!m_chunkList.empty())
	{
		chunk = std::move(m_chunkList.front());
	}
	return chunk;
}

void MemoryPool::put(MemoryChunk_ptr&& chunk)
{
	std::lock_guard<std::mutex> lock(m_chunkLock);
	m_chunkList.push_back(std::move(chunk));
}

std::size_t MemoryPool::chunkSize() const
{
	return m_chunkSize;
}

}