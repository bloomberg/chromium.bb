// Copyright 2016 The SwiftShader Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "Renderer.hpp"

#include "Clipper.hpp"
#include "Primitive.hpp"
#include "Polygon.hpp"
#include "Reactor/Reactor.hpp"
#include "Pipeline/Constants.hpp"
#include "System/Memory.hpp"
#include "System/Half.hpp"
#include "System/Math.hpp"
#include "System/Timer.hpp"
#include "Vulkan/VkConfig.h"
#include "Vulkan/VkDebug.hpp"
#include "Vulkan/VkDevice.hpp"
#include "Vulkan/VkFence.hpp"
#include "Vulkan/VkImageView.hpp"
#include "Vulkan/VkQueryPool.hpp"
#include "Pipeline/SpirvShader.hpp"
#include "Vertex.hpp"

#include "marl/containers.h"
#include "marl/defer.h"
#include "marl/trace.h"

#undef max

#ifndef NDEBUG
unsigned int minPrimitives = 1;
unsigned int maxPrimitives = 1 << 21;
#endif

namespace sw
{
	template<typename T>
	inline bool setBatchIndices(unsigned int batch[128][3], VkPrimitiveTopology topology, VkProvokingVertexModeEXT provokingVertexMode, T indices, unsigned int start, unsigned int triangleCount)
	{
		bool provokeFirst = (provokingVertexMode == VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT);

		switch(topology)
		{
		case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
		{
			auto index = start;
			auto pointBatch = &(batch[0][0]);
			for(unsigned int i = 0; i < triangleCount; i++)
			{
				*pointBatch++ = indices[index++];
			}

			// Repeat the last index to allow for SIMD width overrun.
			index--;
			for(unsigned int i = 0; i < 3; i++)
			{
				*pointBatch++ = indices[index];
			}
			break;
		}
		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
		{
			auto index = 2 * start;
			for(unsigned int i = 0; i < triangleCount; i++)
			{
				batch[i][0] = indices[index + (provokeFirst ? 0 : 1)];
				batch[i][1] = indices[index + (provokeFirst ? 1 : 0)];
				batch[i][2] = indices[index + 1];

				index += 2;
			}
			break;
		}
		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
		{
			auto index = start;
			for(unsigned int i = 0; i < triangleCount; i++)
			{
				batch[i][0] = indices[index + (provokeFirst ? 0 : 1)];
				batch[i][1] = indices[index + (provokeFirst ? 1 : 0)];
				batch[i][2] = indices[index + 1];

				index += 1;
			}
			break;
		}
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
		{
			auto index = 3 * start;
			for(unsigned int i = 0; i < triangleCount; i++)
			{
				batch[i][0] = indices[index + (provokeFirst ? 0 : 2)];
				batch[i][1] = indices[index + (provokeFirst ? 1 : 0)];
				batch[i][2] = indices[index + (provokeFirst ? 2 : 1)];

				index += 3;
			}
			break;
		}
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
		{
			auto index = start;
			for(unsigned int i = 0; i < triangleCount; i++)
			{
				batch[i][0] = indices[index + (provokeFirst ? 0 : 2)];
				batch[i][1] = indices[index + ((start + i) & 1) + (provokeFirst ? 1 : 0)];
				batch[i][2] = indices[index + (~(start + i) & 1) + (provokeFirst ? 1 : 0)];

				index += 1;
			}
			break;
		}
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
		{
			auto index = start + 1;
			for(unsigned int i = 0; i < triangleCount; i++)
			{
				batch[i][provokeFirst ? 0 : 2] = indices[index + 0];
				batch[i][provokeFirst ? 1 : 0] = indices[index + 1];
				batch[i][provokeFirst ? 2 : 1] = indices[0];

				index += 1;
			}
			break;
		}
		default:
			ASSERT(false);
			return false;
		}

		return true;
	}

	DrawCall::DrawCall()
	{
		data = (DrawData*)allocate(sizeof(DrawData));
		data->constants = &constants;
	}

	DrawCall::~DrawCall()
	{
		deallocate(data);
	}

	Renderer::Renderer(vk::Device* device) : device(device)
	{
		VertexProcessor::setRoutineCacheSize(1024);
		PixelProcessor::setRoutineCacheSize(1024);
		SetupProcessor::setRoutineCacheSize(1024);
	}

	Renderer::~Renderer()
	{
		drawTickets.take().wait();
	}

	// Renderer objects have to be mem aligned to the alignment provided in the class declaration
	void* Renderer::operator new(size_t size)
	{
		ASSERT(size == sizeof(Renderer));  // This operator can't be called from a derived class
		return vk::allocate(sizeof(Renderer), alignof(Renderer), vk::DEVICE_MEMORY, VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);
	}

	void Renderer::operator delete(void* mem)
	{
		vk::deallocate(mem, vk::DEVICE_MEMORY);
	}

	void Renderer::draw(const sw::Context* context, VkIndexType indexType, unsigned int count, int baseVertex,
			TaskEvents *events, int instanceID, int viewID, void *indexBuffer, const VkExtent3D& framebufferExtent,
			PushConstantStorage const & pushConstants, bool update)
	{
		if(count == 0) { return; }

		auto id = nextDrawID++;
		MARL_SCOPED_EVENT("draw %d", id);

		#ifndef NDEBUG
		{
			unsigned int minPrimitives = 1;
			unsigned int maxPrimitives = 1 << 21;
			if(count < minPrimitives || count > maxPrimitives)
			{
				return;
			}
		}
		#endif

		int ms = context->sampleCount;

		if(!context->multiSampleMask)
		{
			return;
		}

		marl::Pool<sw::DrawCall>::Loan draw;
		{
			MARL_SCOPED_EVENT("drawCallPool.borrow()");
			draw = drawCallPool.borrow();
		}
		draw->id = id;

		if(update)
		{
			MARL_SCOPED_EVENT("update");
			vertexState = VertexProcessor::update(context);
			setupState = SetupProcessor::update(context);
			pixelState = PixelProcessor::update(context);

			vertexRoutine = VertexProcessor::routine(vertexState, context->pipelineLayout, context->vertexShader, context->descriptorSets);
			setupRoutine = SetupProcessor::routine(setupState);
			pixelRoutine = PixelProcessor::routine(pixelState, context->pipelineLayout, context->pixelShader, context->descriptorSets);
		}

		DrawCall::SetupFunction setupPrimitives = nullptr;
		unsigned int numPrimitivesPerBatch = MaxBatchSize / ms;

		if(context->isDrawTriangle(false))
		{
			switch(context->polygonMode)
			{
			case VK_POLYGON_MODE_FILL:
				setupPrimitives = &DrawCall::setupSolidTriangles;
				break;
			case VK_POLYGON_MODE_LINE:
				setupPrimitives = &DrawCall::setupWireframeTriangles;
				numPrimitivesPerBatch /= 3;
				break;
			case VK_POLYGON_MODE_POINT:
				setupPrimitives = &DrawCall::setupPointTriangles;
				numPrimitivesPerBatch /= 3;
				break;
			default:
				UNSUPPORTED("polygon mode: %d", int(context->polygonMode));
				return;
			}
		}
		else if(context->isDrawLine(false))
		{
			setupPrimitives = &DrawCall::setupLines;
		}
		else  // Point primitive topology
		{
			setupPrimitives = &DrawCall::setupPoints;
		}

		DrawData *data = draw->data;
		draw->occlusionQuery = occlusionQuery;
		draw->batchDataPool = &batchDataPool;
		draw->numPrimitives = count;
		draw->numPrimitivesPerBatch = numPrimitivesPerBatch;
		draw->numBatches = (count + draw->numPrimitivesPerBatch - 1) / draw->numPrimitivesPerBatch;
		draw->topology = context->topology;
		draw->provokingVertexMode = context->provokingVertexMode;
		draw->indexType = indexType;
		draw->lineRasterizationMode = context->lineRasterizationMode;

		draw->vertexRoutine = vertexRoutine;
		draw->setupRoutine = setupRoutine;
		draw->pixelRoutine = pixelRoutine;
		draw->setupPrimitives = setupPrimitives;
		draw->setupState = setupState;

		data->descriptorSets = context->descriptorSets;
		data->descriptorDynamicOffsets = context->descriptorDynamicOffsets;

		for(int i = 0; i < MAX_INTERFACE_COMPONENTS/4; i++)
		{
			data->input[i] = context->input[i].buffer;
			data->robustnessSize[i] = context->input[i].robustnessSize;
			data->stride[i] = context->input[i].vertexStride;
		}

		data->indices = indexBuffer;
		data->viewID = viewID;
		data->instanceID = instanceID;
		data->baseVertex = baseVertex;

		if(pixelState.stencilActive)
		{
			data->stencil[0].set(context->frontStencil.reference, context->frontStencil.compareMask, context->frontStencil.writeMask);
			data->stencil[1].set(context->backStencil.reference, context->backStencil.compareMask, context->backStencil.writeMask);
		}

		data->lineWidth = context->lineWidth;

		data->factor = factor;

		if(pixelState.alphaToCoverage)
		{
			if(ms == 4)
			{
				data->a2c0 = replicate(0.2f);
				data->a2c1 = replicate(0.4f);
				data->a2c2 = replicate(0.6f);
				data->a2c3 = replicate(0.8f);
			}
			else if(ms == 2)
			{
				data->a2c0 = replicate(0.25f);
				data->a2c1 = replicate(0.75f);
			}
			else ASSERT(false);
		}

		if(pixelState.occlusionEnabled)
		{
			for(int cluster = 0; cluster < MaxClusterCount; cluster++)
			{
				data->occlusion[cluster] = 0;
			}
		}

		// Viewport
		{
			float W = 0.5f * viewport.width;
			float H = 0.5f * viewport.height;
			float X0 = viewport.x + W;
			float Y0 = viewport.y + H;
			float N = viewport.minDepth;
			float F = viewport.maxDepth;
			float Z = F - N;
			constexpr float subPixF = vk::SUBPIXEL_PRECISION_FACTOR;

			if(context->isDrawTriangle(false))
			{
				N += context->depthBias;
			}

			data->WxF = replicate(W * subPixF);
			data->HxF = replicate(H * subPixF);
			data->X0xF = replicate(X0 * subPixF - subPixF / 2);
			data->Y0xF = replicate(Y0 * subPixF - subPixF / 2);
			data->halfPixelX = replicate(0.5f / W);
			data->halfPixelY = replicate(0.5f / H);
			data->viewportHeight = abs(viewport.height);
			data->slopeDepthBias = context->slopeDepthBias;
			data->depthRange = Z;
			data->depthNear = N;
		}

		// Target
		{
			for(int index = 0; index < RENDERTARGETS; index++)
			{
				draw->renderTarget[index] = context->renderTarget[index];

				if(draw->renderTarget[index])
				{
					data->colorBuffer[index] = (unsigned int*)context->renderTarget[index]->getOffsetPointer({0, 0, 0}, VK_IMAGE_ASPECT_COLOR_BIT, 0, data->viewID);
					data->colorPitchB[index] = context->renderTarget[index]->rowPitchBytes(VK_IMAGE_ASPECT_COLOR_BIT, 0);
					data->colorSliceB[index] = context->renderTarget[index]->slicePitchBytes(VK_IMAGE_ASPECT_COLOR_BIT, 0);
				}
			}

			draw->depthBuffer = context->depthBuffer;
			draw->stencilBuffer = context->stencilBuffer;

			if(draw->depthBuffer)
			{
				data->depthBuffer = (float*)context->depthBuffer->getOffsetPointer({0, 0, 0}, VK_IMAGE_ASPECT_DEPTH_BIT, 0, data->viewID);
				data->depthPitchB = context->depthBuffer->rowPitchBytes(VK_IMAGE_ASPECT_DEPTH_BIT, 0);
				data->depthSliceB = context->depthBuffer->slicePitchBytes(VK_IMAGE_ASPECT_DEPTH_BIT, 0);
			}

			if(draw->stencilBuffer)
			{
				data->stencilBuffer = (unsigned char*)context->stencilBuffer->getOffsetPointer({0, 0, 0}, VK_IMAGE_ASPECT_STENCIL_BIT, 0, data->viewID);
				data->stencilPitchB = context->stencilBuffer->rowPitchBytes(VK_IMAGE_ASPECT_STENCIL_BIT, 0);
				data->stencilSliceB = context->stencilBuffer->slicePitchBytes(VK_IMAGE_ASPECT_STENCIL_BIT, 0);
			}
		}

		// Scissor
		{
			data->scissorX0 = clamp<int>(scissor.offset.x, 0, framebufferExtent.width);
			data->scissorX1 = clamp<int>(scissor.offset.x + scissor.extent.width, 0, framebufferExtent.width);
			data->scissorY0 = clamp<int>(scissor.offset.y, 0, framebufferExtent.height);
			data->scissorY1 = clamp<int>(scissor.offset.y + scissor.extent.height, 0, framebufferExtent.height);
		}

		// Push constants
		{
			data->pushConstants = pushConstants;
		}

		draw->events = events;

		DrawCall::run(draw, &drawTickets, clusterQueues);
	}

	void DrawCall::setup()
	{
		if(occlusionQuery != nullptr)
		{
			occlusionQuery->start();
		}

		if(events)
		{
			events->start();
		}
	}

	void DrawCall::teardown()
	{
		if(events)
		{
			events->finish();
			events = nullptr;
		}

		if (occlusionQuery != nullptr)
		{
			for(int cluster = 0; cluster < MaxClusterCount; cluster++)
			{
				occlusionQuery->add(data->occlusion[cluster]);
			}
			occlusionQuery->finish();
		}

		vertexRoutine = {};
		setupRoutine = {};
		pixelRoutine = {};
	}

	void DrawCall::run(const marl::Loan<DrawCall>& draw, marl::Ticket::Queue* tickets, marl::Ticket::Queue clusterQueues[MaxClusterCount])
	{
		draw->setup();

		auto const numPrimitives = draw->numPrimitives;
		auto const numPrimitivesPerBatch = draw->numPrimitivesPerBatch;
		auto const numBatches = draw->numBatches;

		auto ticket = tickets->take();
		auto finally = marl::make_shared_finally([draw, ticket] {
			MARL_SCOPED_EVENT("FINISH draw %d", draw->id);
			draw->teardown();
			ticket.done();
		});

		for (unsigned int batchId = 0; batchId < numBatches; batchId++)
		{
			auto batch = draw->batchDataPool->borrow();
			batch->id = batchId;
			batch->firstPrimitive = batch->id * numPrimitivesPerBatch;
			batch->numPrimitives = std::min(batch->firstPrimitive + numPrimitivesPerBatch, numPrimitives) - batch->firstPrimitive;

			for (int cluster = 0; cluster < MaxClusterCount; cluster++)
			{
				batch->clusterTickets[cluster] = std::move(clusterQueues[cluster].take());
			}

			marl::schedule([draw, batch, finally] {

				processVertices(draw.get(), batch.get());

				if (!draw->setupState.rasterizerDiscard)
				{
					processPrimitives(draw.get(), batch.get());

					if (batch->numVisible > 0)
					{
						processPixels(draw, batch, finally);
						return;
					}
				}

				for (int cluster = 0; cluster < MaxClusterCount; cluster++)
				{
					batch->clusterTickets[cluster].done();
				}
			});
		}
	}

	void DrawCall::processVertices(DrawCall* draw, BatchData* batch)
	{
		MARL_SCOPED_EVENT("VERTEX draw %d, batch %d", draw->id, batch->id);

		unsigned int triangleIndices[MaxBatchSize + 1][3];  // One extra for SIMD width overrun. TODO: Adjust to dynamic batch size.
		{
			MARL_SCOPED_EVENT("processPrimitiveVertices");
			processPrimitiveVertices(
				triangleIndices,
				draw->data->indices,
				draw->indexType,
				batch->firstPrimitive,
				batch->numPrimitives,
				draw->topology,
				draw->provokingVertexMode);
		}

		auto& vertexTask = batch->vertexTask;
		vertexTask.primitiveStart = batch->firstPrimitive;
		// We're only using batch compaction for points, not lines
		vertexTask.vertexCount = batch->numPrimitives * ((draw->topology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST) ? 1 : 3);
		if (vertexTask.vertexCache.drawCall != draw->id)
		{
			vertexTask.vertexCache.clear();
			vertexTask.vertexCache.drawCall = draw->id;
		}

		draw->vertexRoutine(&batch->triangles.front().v0, &triangleIndices[0][0], &vertexTask, draw->data);
	}

	void DrawCall::processPrimitives(DrawCall* draw, BatchData* batch)
	{
		MARL_SCOPED_EVENT("PRIMITIVES draw %d batch %d", draw->id, batch->id);
		auto triangles = &batch->triangles[0];
		auto primitives = &batch->primitives[0];
		batch->numVisible = draw->setupPrimitives(triangles, primitives, draw, batch->numPrimitives);
	}

	void DrawCall::processPixels(const marl::Loan<DrawCall>& draw, const marl::Loan<BatchData>& batch, const std::shared_ptr<marl::Finally>& finally)
	{
		struct Data
		{
			Data(const marl::Loan<DrawCall>& draw, const marl::Loan<BatchData>& batch, const std::shared_ptr<marl::Finally>& finally)
				: draw(draw), batch(batch), finally(finally) {}
			marl::Loan<DrawCall> draw;
			marl::Loan<BatchData> batch;
			std::shared_ptr<marl::Finally> finally;
		};
		auto data = std::make_shared<Data>(draw, batch, finally);
		for (int cluster = 0; cluster < MaxClusterCount; cluster++)
		{
			batch->clusterTickets[cluster].onCall([data, cluster]
			{
				auto& draw = data->draw;
				auto& batch = data->batch;
				MARL_SCOPED_EVENT("PIXEL draw %d, batch %d, cluster %d", draw->id, batch->id, cluster);
				draw->pixelRoutine(&batch->primitives.front(), batch->numVisible, cluster, MaxClusterCount, draw->data);
				batch->clusterTickets[cluster].done();
			});
		}
	}

	void Renderer::synchronize()
	{
		MARL_SCOPED_EVENT("synchronize");
		auto ticket = drawTickets.take();
		ticket.wait();
		device->updateSamplingRoutineConstCache();
		ticket.done();
	}

	void DrawCall::processPrimitiveVertices(
		unsigned int triangleIndicesOut[MaxBatchSize + 1][3],
		const void *primitiveIndices,
		VkIndexType indexType,
		unsigned int start,
		unsigned int triangleCount,
		VkPrimitiveTopology topology,
		VkProvokingVertexModeEXT provokingVertexMode)
	{
		if(!primitiveIndices)
		{
			struct LinearIndex
			{
				unsigned int operator[](unsigned int i) { return i; }
			};

			if(!setBatchIndices(triangleIndicesOut, topology, provokingVertexMode, LinearIndex(), start, triangleCount))
			{
				return;
			}
		}
		else
		{
			switch(indexType)
			{
			case VK_INDEX_TYPE_UINT16:
				if(!setBatchIndices(triangleIndicesOut, topology, provokingVertexMode, static_cast<const uint16_t*>(primitiveIndices), start, triangleCount))
				{
					return;
				}
				break;
			case VK_INDEX_TYPE_UINT32:
				if(!setBatchIndices(triangleIndicesOut, topology, provokingVertexMode, static_cast<const uint32_t*>(primitiveIndices), start, triangleCount))
				{
					return;
				}
				break;
			break;
			default:
				ASSERT(false);
				return;
			}
		}

		// setBatchIndices() takes care of the point case, since it's different due to the compaction
		if (topology != VK_PRIMITIVE_TOPOLOGY_POINT_LIST)
		{
			// Repeat the last index to allow for SIMD width overrun.
			triangleIndicesOut[triangleCount][0] = triangleIndicesOut[triangleCount - 1][2];
			triangleIndicesOut[triangleCount][1] = triangleIndicesOut[triangleCount - 1][2];
			triangleIndicesOut[triangleCount][2] = triangleIndicesOut[triangleCount - 1][2];
		}
	}

	int DrawCall::setupSolidTriangles(Triangle *triangles, Primitive *primitives, const DrawCall *drawCall, int count)
	{
		auto &state = drawCall->setupState;

		int ms = state.multiSample;
		const DrawData *data = drawCall->data;
		int visible = 0;

		for(int i = 0; i < count; i++, triangles++)
		{
			Vertex &v0 = triangles->v0;
			Vertex &v1 = triangles->v1;
			Vertex &v2 = triangles->v2;

			Polygon polygon(&v0.position, &v1.position, &v2.position);


			if((v0.cullMask | v1.cullMask | v2.cullMask) == 0)
			{
				continue;
			}

			if((v0.clipFlags & v1.clipFlags & v2.clipFlags) != Clipper::CLIP_FINITE)
			{
				continue;
			}

			int clipFlagsOr = v0.clipFlags | v1.clipFlags | v2.clipFlags;
			if(clipFlagsOr != Clipper::CLIP_FINITE)
			{
				if(!Clipper::Clip(polygon, clipFlagsOr, *drawCall))
				{
					continue;
				}
			}

			if(drawCall->setupRoutine(primitives, triangles, &polygon, data))
			{
				primitives += ms;
				visible++;
			}
		}

		return visible;
	}

	int DrawCall::setupWireframeTriangles(Triangle *triangles, Primitive *primitives, const DrawCall *drawCall, int count)
	{
		auto& state = drawCall->setupState;

		int ms = state.multiSample;
		int visible = 0;

		for(int i = 0; i < count; i++)
		{
			const Vertex &v0 = triangles[i].v0;
			const Vertex &v1 = triangles[i].v1;
			const Vertex &v2 = triangles[i].v2;

			float d = (v0.y * v1.x - v0.x * v1.y) * v2.w +
			          (v0.x * v2.y - v0.y * v2.x) * v1.w +
			          (v2.x * v1.y - v1.x * v2.y) * v0.w;

			bool frontFacing = (state.frontFace == VK_FRONT_FACE_COUNTER_CLOCKWISE) ? (d > 0) : (d < 0);
			if(state.cullMode & VK_CULL_MODE_FRONT_BIT)
			{
				if(frontFacing) continue;
			}
			if(state.cullMode & VK_CULL_MODE_BACK_BIT)
			{
				if(!frontFacing) continue;
			}

			Triangle lines[3];
			lines[0].v0 = v0;
			lines[0].v1 = v1;
			lines[1].v0 = v1;
			lines[1].v1 = v2;
			lines[2].v0 = v2;
			lines[2].v1 = v0;

			for(int i = 0; i < 3; i++)
			{
				if(setupLine(*primitives, lines[i], *drawCall))
				{
					primitives += ms;
					visible++;
				}
			}
		}

		return visible;
	}

	int DrawCall::setupPointTriangles(Triangle *triangles, Primitive *primitives, const DrawCall *drawCall, int count)
	{
		auto& state = drawCall->setupState;

		int ms = state.multiSample;
		int visible = 0;

		for(int i = 0; i < count; i++)
		{
			const Vertex &v0 = triangles[i].v0;
			const Vertex &v1 = triangles[i].v1;
			const Vertex &v2 = triangles[i].v2;

			float d = (v0.y * v1.x - v0.x * v1.y) * v2.w +
			          (v0.x * v2.y - v0.y * v2.x) * v1.w +
			          (v2.x * v1.y - v1.x * v2.y) * v0.w;

			bool frontFacing = (state.frontFace == VK_FRONT_FACE_COUNTER_CLOCKWISE) ? (d > 0) : (d < 0);
			if(state.cullMode & VK_CULL_MODE_FRONT_BIT)
			{
				if(frontFacing) continue;
			}
			if(state.cullMode & VK_CULL_MODE_BACK_BIT)
			{
				if(!frontFacing) continue;
			}

			Triangle points[3];
			points[0].v0 = v0;
			points[1].v0 = v1;
			points[2].v0 = v2;

			for(int i = 0; i < 3; i++)
			{
				if(setupPoint(*primitives, points[i], *drawCall))
				{
					primitives += ms;
					visible++;
				}
			}
		}

		return visible;
	}

	int DrawCall::setupLines(Triangle *triangles, Primitive *primitives, const DrawCall *drawCall, int count)
	{
		auto &state = drawCall->setupState;

		int visible = 0;
		int ms = state.multiSample;

		for(int i = 0; i < count; i++)
		{
			if(setupLine(*primitives, *triangles, *drawCall))
			{
				primitives += ms;
				visible++;
			}

			triangles++;
		}

		return visible;
	}

	int DrawCall::setupPoints(Triangle *triangles, Primitive *primitives, const DrawCall *drawCall, int count)
	{
		auto &state = drawCall->setupState;

		int visible = 0;
		int ms = state.multiSample;

		for(int i = 0; i < count; i++)
		{
			if(setupPoint(*primitives, *triangles, *drawCall))
			{
				primitives += ms;
				visible++;
			}

			triangles++;
		}

		return visible;
	}

	bool DrawCall::setupLine(Primitive &primitive, Triangle &triangle, const DrawCall &draw)
	{
		const DrawData &data = *draw.data;

		float lineWidth = data.lineWidth;

		Vertex &v0 = triangle.v0;
		Vertex &v1 = triangle.v1;

		if((v0.cullMask | v1.cullMask) == 0)
		{
			return false;
		}

		const float4 &P0 = v0.position;
		const float4 &P1 = v1.position;

		if(P0.w <= 0 && P1.w <= 0)
		{
			return false;
		}

		constexpr float subPixF = vk::SUBPIXEL_PRECISION_FACTOR;

		const float W = data.WxF[0] * (1.0f / subPixF);
		const float H = data.HxF[0] * (1.0f / subPixF);

		float dx = W * (P1.x / P1.w - P0.x / P0.w);
		float dy = H * (P1.y / P1.w - P0.y / P0.w);

		if(dx == 0 && dy == 0)
		{
			return false;
		}

		if(draw.lineRasterizationMode != VK_LINE_RASTERIZATION_MODE_BRESENHAM_EXT)
		{
			// Rectangle centered on the line segment

			float4 P[4];
			int C[4];

			P[0] = P0;
			P[1] = P1;
			P[2] = P1;
			P[3] = P0;

			float scale = lineWidth * 0.5f / sqrt(dx*dx + dy*dy);

			dx *= scale;
			dy *= scale;

			float dx0h = dx * P0.w / H;
			float dy0w = dy * P0.w / W;

			float dx1h = dx * P1.w / H;
			float dy1w = dy * P1.w / W;

			P[0].x += -dy0w;
			P[0].y += +dx0h;
			C[0] = Clipper::ComputeClipFlags(P[0]);

			P[1].x += -dy1w;
			P[1].y += +dx1h;
			C[1] = Clipper::ComputeClipFlags(P[1]);

			P[2].x += +dy1w;
			P[2].y += -dx1h;
			C[2] = Clipper::ComputeClipFlags(P[2]);

			P[3].x += +dy0w;
			P[3].y += -dx0h;
			C[3] = Clipper::ComputeClipFlags(P[3]);

			if((C[0] & C[1] & C[2] & C[3]) == Clipper::CLIP_FINITE)
			{
				Polygon polygon(P, 4);

				int clipFlagsOr = C[0] | C[1] | C[2] | C[3];

				if(clipFlagsOr != Clipper::CLIP_FINITE)
				{
					if(!Clipper::Clip(polygon, clipFlagsOr, draw))
					{
						return false;
					}
				}

				return draw.setupRoutine(&primitive, &triangle, &polygon, &data);
			}
		}
		else if(false)  // TODO(b/80135519): Deprecate
		{
			// Connecting diamonds polygon
			// This shape satisfies the diamond test convention, except for the exit rule part.
			// Line segments with overlapping endpoints have duplicate fragments.
			// The ideal algorithm requires half-open line rasterization (b/80135519).

			float4 P[8];
			int C[8];

			P[0] = P0;
			P[1] = P0;
			P[2] = P0;
			P[3] = P0;
			P[4] = P1;
			P[5] = P1;
			P[6] = P1;
			P[7] = P1;

			float dx0 = lineWidth * 0.5f * P0.w / W;
			float dy0 = lineWidth * 0.5f * P0.w / H;

			float dx1 = lineWidth * 0.5f * P1.w / W;
			float dy1 = lineWidth * 0.5f * P1.w / H;

			P[0].x += -dx0;
			C[0] = Clipper::ComputeClipFlags(P[0]);

			P[1].y += +dy0;
			C[1] = Clipper::ComputeClipFlags(P[1]);

			P[2].x += +dx0;
			C[2] = Clipper::ComputeClipFlags(P[2]);

			P[3].y += -dy0;
			C[3] = Clipper::ComputeClipFlags(P[3]);

			P[4].x += -dx1;
			C[4] = Clipper::ComputeClipFlags(P[4]);

			P[5].y += +dy1;
			C[5] = Clipper::ComputeClipFlags(P[5]);

			P[6].x += +dx1;
			C[6] = Clipper::ComputeClipFlags(P[6]);

			P[7].y += -dy1;
			C[7] = Clipper::ComputeClipFlags(P[7]);

			if((C[0] & C[1] & C[2] & C[3] & C[4] & C[5] & C[6] & C[7]) == Clipper::CLIP_FINITE)
			{
				float4 L[6];

				if(dx > -dy)
				{
					if(dx > dy)   // Right
					{
						L[0] = P[0];
						L[1] = P[1];
						L[2] = P[5];
						L[3] = P[6];
						L[4] = P[7];
						L[5] = P[3];
					}
					else   // Down
					{
						L[0] = P[0];
						L[1] = P[4];
						L[2] = P[5];
						L[3] = P[6];
						L[4] = P[2];
						L[5] = P[3];
					}
				}
				else
				{
					if(dx > dy)   // Up
					{
						L[0] = P[0];
						L[1] = P[1];
						L[2] = P[2];
						L[3] = P[6];
						L[4] = P[7];
						L[5] = P[4];
					}
					else   // Left
					{
						L[0] = P[1];
						L[1] = P[2];
						L[2] = P[3];
						L[3] = P[7];
						L[4] = P[4];
						L[5] = P[5];
					}
				}

				Polygon polygon(L, 6);

				int clipFlagsOr = C[0] | C[1] | C[2] | C[3] | C[4] | C[5] | C[6] | C[7];

				if(clipFlagsOr != Clipper::CLIP_FINITE)
				{
					if(!Clipper::Clip(polygon, clipFlagsOr, draw))
					{
						return false;
					}
				}

				return draw.setupRoutine(&primitive, &triangle, &polygon, &data);
			}
		}
		else
		{
			// Parallelogram approximating Bresenham line
			// This algorithm does not satisfy the ideal diamond-exit rule, but does avoid the
			// duplicate fragment rasterization problem and satisfies all of Vulkan's minimum
			// requirements for Bresenham line segment rasterization.

			float4 P[8];
			P[0] = P0;
			P[1] = P0;
			P[2] = P0;
			P[3] = P0;
			P[4] = P1;
			P[5] = P1;
			P[6] = P1;
			P[7] = P1;

			float dx0 = lineWidth * 0.5f * P0.w / W;
			float dy0 = lineWidth * 0.5f * P0.w / H;

			float dx1 = lineWidth * 0.5f * P1.w / W;
			float dy1 = lineWidth * 0.5f * P1.w / H;

			P[0].x += -dx0;
			P[1].y += +dy0;
			P[2].x += +dx0;
			P[3].y += -dy0;
			P[4].x += -dx1;
			P[5].y += +dy1;
			P[6].x += +dx1;
			P[7].y += -dy1;

			float4 L[4];

			if(dx > -dy)
			{
				if(dx > dy)   // Right
				{
					L[0] = P[1];
					L[1] = P[5];
					L[2] = P[7];
					L[3] = P[3];
				}
				else   // Down
				{
					L[0] = P[0];
					L[1] = P[4];
					L[2] = P[6];
					L[3] = P[2];
				}
			}
			else
			{
				if(dx > dy)   // Up
				{
					L[0] = P[0];
					L[1] = P[2];
					L[2] = P[6];
					L[3] = P[4];
				}
				else   // Left
				{
					L[0] = P[1];
					L[1] = P[3];
					L[2] = P[7];
					L[3] = P[5];
				}
			}

			int C0 = Clipper::ComputeClipFlags(L[0]);
			int C1 = Clipper::ComputeClipFlags(L[1]);
			int C2 = Clipper::ComputeClipFlags(L[2]);
			int C3 = Clipper::ComputeClipFlags(L[3]);

			if((C0 & C1 & C2 & C3) == Clipper::CLIP_FINITE)
			{
				Polygon polygon(L, 4);

				int clipFlagsOr = C0 | C1 | C2 | C3;

				if(clipFlagsOr != Clipper::CLIP_FINITE)
				{
					if(!Clipper::Clip(polygon, clipFlagsOr, draw))
					{
						return false;
					}
				}

				return draw.setupRoutine(&primitive, &triangle, &polygon, &data);
			}
		}

		return false;
	}

	bool DrawCall::setupPoint(Primitive &primitive, Triangle &triangle, const DrawCall &draw)
	{
		const DrawData &data = *draw.data;

		Vertex &v = triangle.v0;

		if(v.cullMask == 0)
		{
			return false;
		}

		float pSize = v.pointSize;

		pSize = clamp(pSize, 1.0f, static_cast<float>(vk::MAX_POINT_SIZE));

		float4 P[4];
		int C[4];

		P[0] = v.position;
		P[1] = v.position;
		P[2] = v.position;
		P[3] = v.position;

		const float X = pSize * P[0].w * data.halfPixelX[0];
		const float Y = pSize * P[0].w * data.halfPixelY[0];

		P[0].x -= X;
		P[0].y += Y;
		C[0] = Clipper::ComputeClipFlags(P[0]);

		P[1].x += X;
		P[1].y += Y;
		C[1] = Clipper::ComputeClipFlags(P[1]);

		P[2].x += X;
		P[2].y -= Y;
		C[2] = Clipper::ComputeClipFlags(P[2]);

		P[3].x -= X;
		P[3].y -= Y;
		C[3] = Clipper::ComputeClipFlags(P[3]);

		Polygon polygon(P, 4);

		if((C[0] & C[1] & C[2] & C[3]) == Clipper::CLIP_FINITE)
		{
			int clipFlagsOr = C[0] | C[1] | C[2] | C[3];

			if(clipFlagsOr != Clipper::CLIP_FINITE)
			{
				if(!Clipper::Clip(polygon, clipFlagsOr, draw))
				{
					return false;
				}
			}

			triangle.v1 = triangle.v0;
			triangle.v2 = triangle.v0;

			constexpr float subPixF = vk::SUBPIXEL_PRECISION_FACTOR;

			triangle.v1.projected.x += iround(subPixF * 0.5f * pSize);
			triangle.v2.projected.y -= iround(subPixF * 0.5f * pSize) * (data.HxF[0] > 0.0f ? 1 : -1);   // Both Direct3D and OpenGL expect (0, 0) in the top-left corner
			return draw.setupRoutine(&primitive, &triangle, &polygon, &data);
		}

		return false;
	}

	void Renderer::addQuery(vk::Query *query)
	{
		ASSERT(query->getType() == VK_QUERY_TYPE_OCCLUSION);
		ASSERT(!occlusionQuery);

		occlusionQuery = query;
	}

	void Renderer::removeQuery(vk::Query *query)
	{
		ASSERT(query->getType() == VK_QUERY_TYPE_OCCLUSION);
		ASSERT(occlusionQuery == query);

		occlusionQuery = nullptr;
	}

	void Renderer::advanceInstanceAttributes(Stream* inputs)
	{
		for(uint32_t i = 0; i < vk::MAX_VERTEX_INPUT_BINDINGS; i++)
		{
			auto &attrib = inputs[i];
			if (attrib.count && attrib.instanceStride && (attrib.instanceStride < attrib.robustnessSize))
			{
				// Under the casts: attrib.buffer += attrib.instanceStride
				attrib.buffer = (void const *)((uintptr_t)attrib.buffer + attrib.instanceStride);
				attrib.robustnessSize -= attrib.instanceStride;
			}
		}
	}

	void Renderer::setViewport(const VkViewport &viewport)
	{
		this->viewport = viewport;
	}

	void Renderer::setScissor(const VkRect2D &scissor)
	{
		this->scissor = scissor;
	}

}
