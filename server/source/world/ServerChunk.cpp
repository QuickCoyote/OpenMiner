/*
 * =====================================================================================
 *
 *       Filename:  ServerChunk.cpp
 *
 *    Description:
 *
 *        Created:  12/01/2019 01:33:16
 *
 *         Author:  Quentin Bazin, <quent42340@gmail.com>
 *
 * =====================================================================================
 */
#include <cstring>

#include "ServerChunk.hpp"

ServerChunk::ServerChunk(s32 x, s32 y, s32 z) : m_x(x), m_y(y), m_z(z) {
	std::memset(m_data, 0, sizeof(m_data));
}

void ServerChunk::generate() {
	if (m_isGenerated) return;

	m_terrainGenerator.generate(*this);

	m_isGenerated = true;
	m_hasChanged = true;
}

u16 ServerChunk::getBlock(int x, int y, int z) const {
	if(x < 0)             return m_surroundingChunks[0] ? m_surroundingChunks[0]->getBlock(x + CHUNK_WIDTH, y, z) : 0;
	if(x >= CHUNK_WIDTH)  return m_surroundingChunks[1] ? m_surroundingChunks[1]->getBlock(x - CHUNK_WIDTH, y, z) : 0;
	if(y < 0)             return m_surroundingChunks[4] ? m_surroundingChunks[4]->getBlock(x, y + CHUNK_HEIGHT, z) : 0;
	if(y >= CHUNK_HEIGHT) return m_surroundingChunks[5] ? m_surroundingChunks[5]->getBlock(x, y - CHUNK_HEIGHT, z) : 0;
	if(z < 0)             return m_surroundingChunks[2] ? m_surroundingChunks[2]->getBlock(x, y, z + CHUNK_DEPTH) : 0;
	if(z >= CHUNK_DEPTH)  return m_surroundingChunks[3] ? m_surroundingChunks[3]->getBlock(x, y, z - CHUNK_DEPTH) : 0;
	return m_data[x][y][z];
}

u16 ServerChunk::getData(int x, int y, int z) const {
	if(x < 0)             return m_surroundingChunks[0] ? m_surroundingChunks[0]->getData(x + CHUNK_WIDTH, y, z) : 0;
	if(x >= CHUNK_WIDTH)  return m_surroundingChunks[1] ? m_surroundingChunks[1]->getData(x - CHUNK_WIDTH, y, z) : 0;
	if(y < 0)             return m_surroundingChunks[4] ? m_surroundingChunks[4]->getData(x, y + CHUNK_HEIGHT, z) : 0;
	if(y >= CHUNK_HEIGHT) return m_surroundingChunks[5] ? m_surroundingChunks[5]->getData(x, y - CHUNK_HEIGHT, z) : 0;
	if(z < 0)             return m_surroundingChunks[2] ? m_surroundingChunks[2]->getData(x, y, z + CHUNK_DEPTH) : 0;
	if(z >= CHUNK_DEPTH)  return m_surroundingChunks[3] ? m_surroundingChunks[3]->getData(x, y, z - CHUNK_DEPTH) : 0;
	return (m_data[x][y][z] >> 16) & 0xffff;
}

void ServerChunk::setBlock(int x, int y, int z, u16 type) {
	if(x < 0)             { if(m_surroundingChunks[0]) m_surroundingChunks[0]->setBlock(x + CHUNK_WIDTH, y, z, type); return; }
	if(x >= CHUNK_WIDTH)  { if(m_surroundingChunks[1]) m_surroundingChunks[1]->setBlock(x - CHUNK_WIDTH, y, z, type); return; }
	if(y < 0)             { if(m_surroundingChunks[4]) m_surroundingChunks[4]->setBlock(x, y + CHUNK_HEIGHT, z, type); return; }
	if(y >= CHUNK_HEIGHT) { if(m_surroundingChunks[5]) m_surroundingChunks[5]->setBlock(x, y - CHUNK_HEIGHT, z, type); return; }
	if(z < 0)             { if(m_surroundingChunks[2]) m_surroundingChunks[2]->setBlock(x, y, z + CHUNK_DEPTH, type); return; }
	if(z >= CHUNK_DEPTH)  { if(m_surroundingChunks[3]) m_surroundingChunks[3]->setBlock(x, y, z - CHUNK_DEPTH, type); return; }

	m_data[x][y][z] = type;

	m_hasChanged = true;
}

void ServerChunk::setData(int x, int y, int z, u16 data) {
	if(x < 0)             { if(m_surroundingChunks[0]) m_surroundingChunks[0]->setData(x + CHUNK_WIDTH, y, z, data); return; }
	if(x >= CHUNK_WIDTH)  { if(m_surroundingChunks[1]) m_surroundingChunks[1]->setData(x - CHUNK_WIDTH, y, z, data); return; }
	if(y < 0)             { if(m_surroundingChunks[4]) m_surroundingChunks[4]->setData(x, y + CHUNK_HEIGHT, z, data); return; }
	if(y >= CHUNK_HEIGHT) { if(m_surroundingChunks[5]) m_surroundingChunks[5]->setData(x, y - CHUNK_HEIGHT, z, data); return; }
	if(z < 0)             { if(m_surroundingChunks[2]) m_surroundingChunks[2]->setData(x, y, z + CHUNK_DEPTH, data); return; }
	if(z >= CHUNK_DEPTH)  { if(m_surroundingChunks[3]) m_surroundingChunks[3]->setData(x, y, z - CHUNK_DEPTH, data); return; }

	m_data[x][y][z] &= 0xffff;
	m_data[x][y][z] |= (data << 16);

	m_hasChanged = true;
}
