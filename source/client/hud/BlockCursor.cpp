/*
 * =====================================================================================
 *
 *  OpenMiner
 *
 *  Copyright (C) 2018-2020 Unarelith, Quentin Bazin <openminer@unarelith.net>
 *  Copyright (C) 2019-2020 the OpenMiner contributors (see CONTRIBUTORS.md)
 *
 *  This file is part of OpenMiner.
 *
 *  OpenMiner is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  OpenMiner is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with OpenMiner; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 * =====================================================================================
 */
#include <glm/gtc/matrix_transform.hpp>

#include <gk/core/input/GamePad.hpp>
#include <gk/gl/GLCheck.hpp>
#include <gk/gl/Vertex.hpp>
#include <gk/core/GameClock.hpp>
#include <gk/resource/ResourceHandler.hpp>

#include "BlockCursor.hpp"
#include "BlockCursorRaycast.hpp"
#include "ClientCommandHandler.hpp"
#include "ClientPlayer.hpp"
#include "Config.hpp"
#include "GameKey.hpp"
#include "Hotbar.hpp"
#include "Registry.hpp"

BlockCursor::BlockCursor(ClientPlayer &player, ClientWorld &world, ClientCommandHandler &client)
	: m_player(player), m_world(world), m_client(client)
{
	m_blockDestroyTexture = &gk::ResourceHandler::getInstance().get<sf::Texture>("texture-block_destroy");
}

void BlockCursor::onEvent(const sf::Event &event, const Hotbar &hotbar) {
	if (event.type == sf::Event::MouseButtonPressed && m_selectedBlock.w != -1) {
		if (event.mouseButton.button == sf::Mouse::Left) {
			m_animationStart = gk::GameClock::getInstance().getTicks();
			m_currentTool = &m_player.inventory().getStack(hotbar.cursorPos(), 0);
		}
		else if (event.mouseButton.button == sf::Mouse::Right) {
			if (m_animationStart != 0)
				m_animationStart = 0;

			u32 blockId = m_world.getBlock(m_selectedBlock.x, m_selectedBlock.y, m_selectedBlock.z);
			const Block &block = Registry::getInstance().getBlock(blockId);
			const Item &item = Registry::getInstance().getItem(hotbar.currentItem());

			bool blockActivationSent = false;
			if (block.id() && block.canBeActivated() && !gk::GamePad::isKeyPressed(GameKey::Sneak)) {
				m_client.sendBlockActivated(m_selectedBlock);
				blockActivationSent = true;
			}

			if (block.id() && !blockActivationSent && hotbar.currentItem() && item.isBlock()) {
				s8 face = m_selectedBlock.w;

				s32 x = m_selectedBlock.x;
				s32 y = m_selectedBlock.y;
				s32 z = m_selectedBlock.z;

				// FIXME: Document where these face numbers come from
				if (face == 0) ++x;
				if (face == 3) --x;
				if (face == 1) ++y;
				if (face == 4) --y;
				if (face == 2) ++z;
				if (face == 5) --z;

				// First, we check if the new block is not replacing another block
				u32 blockId = m_world.getBlock(x, y, z);
				const Block &block = Registry::getInstance().getBlock(blockId);
				if (!blockId || block.drawType() == BlockDrawType::Liquid) {
					// Second, we check if the new block is not inside the player
					const Block &newBlock = Registry::getInstance().getBlock(hotbar.currentItem());
					gk::FloatBox boundingBox = newBlock.boundingBox() + gk::Vector3f(x - m_player.x(), y - m_player.y(), z - m_player.z());
					gk::FloatBox playerBoundingBox = m_player.hitbox();
					if (!boundingBox.intersects(playerBoundingBox)) {
						u32 block = hotbar.currentItem();
						if (newBlock.isRotatable()) {
							u16 data = m_player.getOppositeDirection() & 0x3;
							m_world.setData(x, y, z, data);

							block |= data << 16;
						}

						m_world.setBlock(x, y, z, hotbar.currentItem());

						m_client.sendPlayerPlaceBlock(x, y, z, block);

						const ItemStack &currentStack = m_player.inventory().getStack(hotbar.cursorPos(), 0);
						m_player.inventory().setStack(hotbar.cursorPos(), 0, currentStack.amount() > 1 ? currentStack.item().stringID() : "", currentStack.amount() - 1);

						m_client.sendPlayerInvUpdate();
					}
				}
			}
		}
	}
	else if (event.type == sf::Event::MouseButtonReleased) {
		if (event.mouseButton.button == sf::Mouse::Left) {
			m_animationStart = 0;
		}
	}
}

void BlockCursor::update(const Hotbar &hotbar) {
	bool selectedBlockChanged = false;
	glm::ivec4 selectedBlock = findSelectedBlock();
	if (selectedBlock.x != m_selectedBlock.x || selectedBlock.y != m_selectedBlock.y || selectedBlock.z != m_selectedBlock.z)
		selectedBlockChanged = true;

	m_selectedBlock = selectedBlock;

	m_currentBlock = &Registry::getInstance().getBlock(m_world.getBlock(m_selectedBlock.x, m_selectedBlock.y, m_selectedBlock.z));
	// if (block.boundingBox().intersects(FloatBox{m_selectedBlock.x, m_selectedBlock.y, m_selectedBlock.z, 1, 1, 1})) {
	// 	selectedBlockChanged = false;
	// 	m_selectedBlock.w = -1;
	// }

	u32 ticks = gk::GameClock::getInstance().getTicks();

	if (selectedBlockChanged)
		m_animationStart = (m_animationStart) ? ticks : 0;

	const ItemStack &currentStack = m_player.inventory().getStack(hotbar.cursorPos(), 0);
	float timeToBreak = 0;
	if (m_animationStart) {
		if (m_currentTool->item().id() != currentStack.item().id()) {
			m_animationStart = ticks;
			m_currentTool = &currentStack;
		}
		else {
			timeToBreak = m_currentBlock->timeToBreak(currentStack.item().harvestCapability(), currentStack.item().miningSpeed());

			if (ticks > m_animationStart + timeToBreak * 1000) {
				m_world.setBlock(m_selectedBlock.x, m_selectedBlock.y, m_selectedBlock.z, 0);
				m_animationStart = ticks;

				m_client.sendPlayerDigBlock(m_selectedBlock);
			}
		}
	}

	u8f orientation = m_currentBlock->isRotatable() ? m_world.getData(selectedBlock.x, selectedBlock.y, selectedBlock.z) & 0x1F : 0;

	if (m_selectedBlock.w != -1)
		updateVertexBuffer(*m_currentBlock, orientation);
	else
		m_currentBlock = nullptr;

	if (m_animationStart && m_currentBlock)
		updateAnimationVertexBuffer(*m_currentBlock, orientation,
		                            (ticks - m_animationStart) / (timeToBreak * 100));
}

using namespace BlockGeometry;

void BlockCursor::updateVBOCoords(Vertex vertices[nFaces][nVertsPerFace], const Block &block,
	float face, u8f orientation)
{
	glm::vec3 bottomLeft{block.boundingBox().x, block.boundingBox().y, block.boundingBox().z};
	glm::vec3 topRight{block.boundingBox().sizeX, block.boundingBox().sizeY, block.boundingBox().sizeZ};
	topRight += bottomLeft;

	const glm::mat3 &orientMatrix = orientMatrices[orientation];

	glm::vec3 vertexPos[nVertsPerCube]{
		// Order is important. It matches the bit order defined in BlockGeometry::cubeVerts.
		{bottomLeft.x, bottomLeft.y, bottomLeft.z},
		{topRight.x,   bottomLeft.y, bottomLeft.z},
		{bottomLeft.x, topRight.y,   bottomLeft.z},
		{topRight.x,   topRight.y,   bottomLeft.z},
		{bottomLeft.x, bottomLeft.y, topRight.z},
		{topRight.x,   bottomLeft.y, topRight.z},
		{bottomLeft.x, topRight.y,   topRight.z},
		{topRight.x,   topRight.y,   topRight.z},
	};

	if (orientation != 0) {
		static const glm::vec3 half{0.5, 0.5, 0.5};
		// Rotate each vertex coordinate around the centre of the cube
		for (int i = 0; i < nVertsPerCube; ++i) {
			vertexPos[i] = orientMatrix * (vertexPos[i] - half) + half;
		}
	}

	for (u8f f = 0 ; f < nFaces; ++f) {
		for (u8f v = 0; v < nVertsPerFace; ++v) {
			vertices[f][v].coord3d[0] = vertexPos[cubeVerts[f][v]].x;
			vertices[f][v].coord3d[1] = vertexPos[cubeVerts[f][v]].y;
			vertices[f][v].coord3d[2] = vertexPos[cubeVerts[f][v]].z;
			vertices[f][v].coord3d[3] = face;
		}
	}
}

void BlockCursor::updateVertexBuffer(const Block &block, u8f orientation) {
	Vertex vertices[nFaces][nVertsPerFace];
	updateVBOCoords(vertices, block, -1, orientation);

	gk::VertexBuffer::bind(&m_vbo);
	m_vbo.setData(sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
	gk::VertexBuffer::bind(nullptr);
}

void BlockCursor::updateAnimationVertexBuffer(const Block &block, u8f orientation, int animationPos) {
	Vertex vertices[nFaces][nVertsPerFace];
	updateVBOCoords(vertices, block, -2, orientation);

	GLfloat color[4] = {1, 1, 1, 0.5};
	for (u8f f = 0; f < nFaces; ++f)
		for (u8f v = 0; v < nVertsPerFace; ++v)
			memcpy(&vertices[f][v].color, &color[0], 4 * sizeof(GLfloat));

	if (animationPos != -1) {
		glm::vec4 blockTexCoords{0.1f * animationPos, 0.0, 0.1f + 0.1f * animationPos, 1.0};
		float faceTexCoords[nVertsPerFace][nCoordsPerUV] = {
			{blockTexCoords.x, blockTexCoords.w},
			{blockTexCoords.z, blockTexCoords.w},
			{blockTexCoords.z, blockTexCoords.y},
			{blockTexCoords.x, blockTexCoords.y},
		};

		for (u8f f = 0 ; f < nFaces ; ++f) {
			for(u8f v = 0 ; v < nVertsPerFace ; v++) {
				vertices[f][v].texCoord[0] = faceTexCoords[v][0];
				vertices[f][v].texCoord[1] = faceTexCoords[v][1];
			}
		}
	}

	gk::VertexBuffer::bind(&m_animationVBO);
	m_animationVBO.setData(sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
	gk::VertexBuffer::bind(nullptr);
}

void BlockCursor::draw(gk::RenderTarget &target, gk::RenderStates states) const {
	if (m_selectedBlock.w == -1) return;

	states.vertexAttributes = VertexAttribute::All;

	glCheck(glDisable(GL_POLYGON_OFFSET_FILL));
	glCheck(glDisable(GL_CULL_FACE));
	glCheck(glEnable(GL_DEPTH_TEST));

	// Subtract the camera position - see comment in ClientWorld::draw()
	gk::Vector3d cameraPosition = m_player.camera().getDPosition();
	states.transform.translate(m_selectedBlock.x - cameraPosition.x, m_selectedBlock.y - cameraPosition.y, m_selectedBlock.z - cameraPosition.z);

	glCheck(glPolygonMode(GL_FRONT_AND_BACK, GL_LINE));
	target.draw(m_vbo, GL_QUADS, 0, nFaces * nVertsPerFace, states);
	glCheck(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));

	if (m_animationStart > 0) {
		glCheck(glEnable(GL_CULL_FACE));
		glCheck(glBlendFunc(GL_DST_COLOR, GL_ZERO));

		states.texture = m_blockDestroyTexture;

		target.draw(m_animationVBO, GL_QUADS, 0, nFaces * nVertsPerFace, states);

		glCheck(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
	}

	glCheck(glEnable(GL_POLYGON_OFFSET_FILL));
}

glm::ivec4 BlockCursor::findSelectedBlock() const {
	glm::dvec3 position{m_player.camera().getDPosition().x,
	                    m_player.camera().getDPosition().y,
	                    m_player.camera().getDPosition().z};

	s32f bestX = s32f(floor(position.x));
	s32f bestY = s32f(floor(position.y));
	s32f bestZ = s32f(floor(position.z));

	// Deal with a degenerate case: camera in the middle of a block
	u32f blockID = m_world.getBlock(bestX, bestY, bestZ);
	const Block &block = Registry::getInstance().getBlock(blockID);
	if (blockID && block.drawType() != BlockDrawType::Liquid) {
		// We're inside a node, therefore there's no face, but we still need
		// to return a valid block. We use face 6 for that. For rightclicks,
		// it should attempt to place the block on the same node clicked, and
		// fail because it's already occupied. For digs, it should just dig
		// the node.
		return glm::ivec4{bestX, bestY, bestZ, 6};
	}

	// Still the degenerate case, but this time for the sub-case where
	// the coordinates of the player are exact integers, thus lying exactly
	// between two faces (or four, if it's in a corner). Deal with that case
	// here. NOTE: We don't deal with height because it's unlikely that the
	// camera ends up being at an integer position.
	if (double(bestX) == position.x && double(bestY) == position.y) {
		for (int y = -1; y <= 0; y++) {
			for (int x = -1; x <= 0; x++) {
				blockID = m_world.getBlock(bestX + x, bestY + y, bestZ);
				const Block &block = Registry::getInstance().getBlock(blockID);
				if (blockID && block.drawType() != BlockDrawType::Liquid) {
					return glm::ivec4{bestX + x, bestY + y, bestZ, 6};
				}
			}
		}
	}

	glm::dvec3 lookAt{m_player.dirTargetedX(), m_player.dirTargetedY(), m_player.dirTargetedZ()};

	// Ray casting algorithm to find out which block we are looking at
	const double maxReach = 10.;
	double bestDepth;
	s8f bestFace = -1;

	glm::dvec3 lookAtN = glm::normalize(lookAt);

	BlockCursorRaycast::rayCastToAxis(AXIS_X, position, lookAtN, maxReach, bestX, bestY, bestZ, bestFace, bestDepth, m_world);
	BlockCursorRaycast::rayCastToAxis(AXIS_Y, position, lookAtN, maxReach, bestX, bestY, bestZ, bestFace, bestDepth, m_world);
	BlockCursorRaycast::rayCastToAxis(AXIS_Z, position, lookAtN, maxReach, bestX, bestY, bestZ, bestFace, bestDepth, m_world);

	return glm::ivec4{bestX, bestY, bestZ, bestFace};
}
