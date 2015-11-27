

#include "mempool.hpp"
#include <cstring>

namespace ics {

MemoryChunk::MemoryChunk()
	: data(nullptr), length(0), size(0)
{

}

MemoryChunk::MemoryChunk(void* buf, std::size_t usedLength, std::size_t totalSize)
	: data((uint8_t*)buf), length(usedLength), size(totalSize)
{

}


MemoryChunk::MemoryChunk(const MemoryChunk& rhs)
	: data(rhs.data), length(rhs.length), size(rhs.size)
{
}


MemoryChunk::MemoryChunk(MemoryChunk&& rhs)
: data(rhs.data), length(rhs.length), size(rhs.size)
{
	rhs.size = 0;
}


MemoryChunk::~MemoryChunk()
{

}

MemoryChunk MemoryChunk::clone(MemoryPool& mp)
{
	auto mc = mp.get();
	mc.length = length;
	std::memcpy(mc.data, data, mc.length);
	return mc;
}

MemoryChunk& MemoryChunk::operator = (MemoryChunk&& rhs)
{
	data = rhs.data;
	length = rhs.length;
	size = rhs.size;
	rhs.size = 0;
	return *this;
}

MemoryChunk& MemoryChunk::operator = (const MemoryChunk& rhs)
{
	data = rhs.data;
	length = rhs.length;
	size = rhs.size;
	return *this;
}


bool MemoryChunk::valid() const
{
	return !data && size > 0;
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
	m_buff = new uint8_t[m_chunkSize * m_chunkCount];

	if (zeroData)
	{
		std::memset(m_buff, 0, m_chunkSize * m_chunkCount);
	}

	for (size_t i = 0; i < m_chunkCount; i++)
	{
		m_chunkList.push_back(MemoryChunk(m_buff + i*m_chunkSize, 0, m_chunkSize));
	}
}

MemoryChunk MemoryPool::get()
{
	std::lock_guard<std::mutex> lock(m_chunkLock);
	MemoryChunk chunk;
	if (!m_chunkList.empty())
	{
		chunk =m_chunkList.front();
	}
	return chunk;
}

void MemoryPool::put(MemoryChunk&& chunk)
{
	if (chunk.valid())
	{
		std::lock_guard<std::mutex> lock(m_chunkLock);
		m_chunkList.push_back(chunk);
	}
}

std::size_t MemoryPool::chunkSize() const
{
	return m_chunkSize;
}

}