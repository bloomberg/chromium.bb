/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Vulkan Fill Buffer Tests
 *//*--------------------------------------------------------------------*/

#include "vktApiFillBufferTests.hpp"
#include "vktApiBufferAndImageAllocationUtil.hpp"

#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkCmdUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuVectorType.hpp"
#include "deSharedPtr.hpp"
#include <limits>

namespace vkt
{

namespace api
{

using namespace vk;

namespace
{

struct TestParams
{
	enum
	{
		TEST_DATA_SIZE													= 256
	};

	VkDeviceSize					dstSize;
	VkDeviceSize					dstOffset;
	VkDeviceSize					size;
	deUint32						testData[TEST_DATA_SIZE];
	de::SharedPtr<IBufferAllocator>	bufferAllocator;
};


class FillWholeBufferTestInstance : public vkt::TestInstance
{
public:
							FillWholeBufferTestInstance	(Context& context, const TestParams& testParams);
	virtual tcu::TestStatus iterate						(void) override;
protected:
	// dstSize will be used as the buffer size.
	// dstOffset will be used as the offset for vkCmdFillBuffer.
	// size in vkCmdFillBuffer will always be VK_WHOLE_SIZE.
	const TestParams		m_params;

	Move<VkCommandPool>		m_cmdPool;
	Move<VkCommandBuffer>	m_cmdBuffer;

	Move<VkBuffer>			m_destination;
	de::MovePtr<Allocation>	m_destinationBufferAlloc;
};

FillWholeBufferTestInstance::FillWholeBufferTestInstance(Context& context, const TestParams& testParams)
	: vkt::TestInstance(context), m_params(testParams)
{
	const DeviceInterface&	vk					= context.getDeviceInterface();
	const VkDevice			vkDevice			= context.getDevice();
	const deUint32			queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
	Allocator&				memAlloc			= context.getDefaultAllocator();

	m_cmdPool = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
	m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	testParams.bufferAllocator->createTestBuffer(m_params.dstSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, context, memAlloc, m_destination, MemoryRequirement::HostVisible, m_destinationBufferAlloc);
}

tcu::TestStatus FillWholeBufferTestInstance::iterate(void)
{
	const DeviceInterface&		vk					= m_context.getDeviceInterface();
	const VkDevice				vkDevice			= m_context.getDevice();
	const VkQueue				queue				= m_context.getUniversalQueue();

	// Make sure some stuff below will work.
	DE_ASSERT(m_params.dstSize >= sizeof(deUint32));
	DE_ASSERT(m_params.dstSize <  static_cast<VkDeviceSize>(std::numeric_limits<size_t>::max()));
	DE_ASSERT(m_params.dstOffset < m_params.dstSize);

	// Fill buffer from the host and flush buffer memory.
	deUint8* bytes = reinterpret_cast<deUint8*>(m_destinationBufferAlloc->getHostPtr());
	deMemset(bytes, 0xff, static_cast<size_t>(m_params.dstSize));
	flushAlloc(vk, vkDevice, *m_destinationBufferAlloc);

	const VkBufferMemoryBarrier	gpuToHostBarrier	=
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			srcAccessMask;
		VK_ACCESS_HOST_READ_BIT,					// VkAccessFlags			dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
		*m_destination,								// VkBuffer					buffer;
		0u,											// VkDeviceSize				offset;
		VK_WHOLE_SIZE								// VkDeviceSize				size;
	};

	// Fill buffer using VK_WHOLE_SIZE.
	beginCommandBuffer(vk, *m_cmdBuffer);
	vk.cmdFillBuffer(*m_cmdBuffer, *m_destination, m_params.dstOffset, VK_WHOLE_SIZE, deUint32{0x01010101});
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0, DE_NULL, 1, &gpuToHostBarrier, 0, DE_NULL);
	endCommandBuffer(vk, *m_cmdBuffer);
	submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());

	// Invalidate buffer memory and check the buffer contains the expected results.
	invalidateAlloc(vk, vkDevice, *m_destinationBufferAlloc);

	const VkDeviceSize startOfExtra = (m_params.dstSize / sizeof(deUint32)) * sizeof(deUint32);
	for (VkDeviceSize i = 0; i < m_params.dstSize; ++i)
	{
		const deUint8 expectedByte = ((i >= m_params.dstOffset && i < startOfExtra)? 0x01 : 0xff);
		if (bytes[i] != expectedByte)
		{
			std::ostringstream msg;
			msg << "Invalid byte at position " << i << " in the buffer (found 0x"
				<< std::hex << static_cast<int>(bytes[i]) << " but expected 0x" << static_cast<int>(expectedByte) << ")";
			return tcu::TestStatus::fail(msg.str());
		}
	}

	return tcu::TestStatus::pass("Pass");
}

class FillWholeBufferTestCase : public vkt::TestCase
{
public:
							FillWholeBufferTestCase	(tcu::TestContext&	testCtx,
													 const std::string&	name,
													 const std::string&	description,
													 const TestParams	params)
		: vkt::TestCase(testCtx, name, description), m_params(params)
	{}

	virtual TestInstance*	createInstance			(Context&			context) const override
	{
		return static_cast<TestInstance*>(new FillWholeBufferTestInstance(context, m_params));
	}
private:
	const TestParams		m_params;
};


class FillBufferTestInstance : public vkt::TestInstance
{
public:
									FillBufferTestInstance				(Context&					context,
																		 TestParams					testParams);
	virtual tcu::TestStatus			iterate								(void);
protected:
	const TestParams				m_params;

	Move<VkCommandPool>				m_cmdPool;
	Move<VkCommandBuffer>			m_cmdBuffer;
	de::MovePtr<tcu::TextureLevel>	m_destinationTextureLevel;
	de::MovePtr<tcu::TextureLevel>	m_expectedTextureLevel;

	VkCommandBufferBeginInfo		m_cmdBufferBeginInfo;

	Move<VkBuffer>					m_destination;
	de::MovePtr<Allocation>			m_destinationBufferAlloc;

	void							generateBuffer						(tcu::PixelBufferAccess		buffer,
																		 int						width,
																		 int						height,
																		 int						depth = 1);
	virtual void					generateExpectedResult				(void);
	void							uploadBuffer						(tcu::ConstPixelBufferAccess
																									bufferAccess,
																		 const Allocation&			bufferAlloc);
	virtual tcu::TestStatus			checkTestResult						(tcu::ConstPixelBufferAccess
																									result);
	deUint32						calculateSize						(tcu::ConstPixelBufferAccess
																									src) const
	{
		return src.getWidth() * src.getHeight() * src.getDepth() * tcu::getPixelSize(src.getFormat());
	}
};

									FillBufferTestInstance::FillBufferTestInstance
																		(Context&					context,
																		 TestParams					testParams)
									: vkt::TestInstance					(context)
									, m_params							(testParams)
{
	const DeviceInterface&			vk									= context.getDeviceInterface();
	const VkDevice					vkDevice							= context.getDevice();
	const deUint32					queueFamilyIndex					= context.getUniversalQueueFamilyIndex();
	Allocator&						memAlloc							= context.getDefaultAllocator();

	// Create command pool
	m_cmdPool = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);

	// Create command buffer
	m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	testParams.bufferAllocator->createTestBuffer(m_params.dstSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, context, memAlloc, m_destination, MemoryRequirement::HostVisible, m_destinationBufferAlloc);
}

tcu::TestStatus						FillBufferTestInstance::iterate		(void)
{
	const int						dstLevelWidth						= (int)(m_params.dstSize / 4);
	m_destinationTextureLevel = de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(mapVkFormat(VK_FORMAT_R8G8B8A8_UINT), dstLevelWidth, 1));

	generateBuffer(m_destinationTextureLevel->getAccess(), dstLevelWidth, 1, 1);

	generateExpectedResult();

	uploadBuffer(m_destinationTextureLevel->getAccess(), *m_destinationBufferAlloc);

	const DeviceInterface&			vk									= m_context.getDeviceInterface();
	const VkDevice					vkDevice							= m_context.getDevice();
	const VkQueue					queue								= m_context.getUniversalQueue();

	const VkBufferMemoryBarrier		dstBufferBarrier					=
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,						// VkStructureType			sType;
		DE_NULL,														// const void*				pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,									// VkAccessFlags			srcAccessMask;
		VK_ACCESS_HOST_READ_BIT,										// VkAccessFlags			dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,										// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,										// deUint32					dstQueueFamilyIndex;
		*m_destination,													// VkBuffer					buffer;
		m_params.dstOffset,												// VkDeviceSize				offset;
		VK_WHOLE_SIZE												// VkDeviceSize				size;
	};

	beginCommandBuffer(vk, *m_cmdBuffer);
	vk.cmdFillBuffer(*m_cmdBuffer, *m_destination, m_params.dstOffset, m_params.size, m_params.testData[0]);
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &dstBufferBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);
	endCommandBuffer(vk, *m_cmdBuffer);

	submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());

	// Read buffer data
	de::MovePtr<tcu::TextureLevel>	resultLevel	(new tcu::TextureLevel(m_destinationTextureLevel->getAccess().getFormat(), dstLevelWidth, 1));
	invalidateAlloc(vk, vkDevice, *m_destinationBufferAlloc);
	tcu::copy(*resultLevel, tcu::ConstPixelBufferAccess(resultLevel->getFormat(), resultLevel->getSize(), m_destinationBufferAlloc->getHostPtr()));

	return checkTestResult(resultLevel->getAccess());
}

void								FillBufferTestInstance::generateBuffer
																		(tcu::PixelBufferAccess		buffer,
																		 int						width,
																		 int						height,
																		 int						depth)
{
	for (int z = 0; z < depth; z++)
	{
		for (int y = 0; y < height; y++)
		{
			for (int x = 0; x < width; x++)
				buffer.setPixel(tcu::UVec4(x, y, z, 255), x, y, z);
		}
	}
}

void								FillBufferTestInstance::uploadBuffer
																		(tcu::ConstPixelBufferAccess
																									bufferAccess,
																		 const Allocation&			bufferAlloc)
{
	const DeviceInterface&			vk									= m_context.getDeviceInterface();
	const VkDevice					vkDevice							= m_context.getDevice();
	const deUint32					bufferSize							= calculateSize(bufferAccess);

	// Write buffer data
	deMemcpy(bufferAlloc.getHostPtr(), bufferAccess.getDataPtr(), bufferSize);
	flushAlloc(vk, vkDevice, bufferAlloc);
}

tcu::TestStatus						FillBufferTestInstance::checkTestResult
																		(tcu::ConstPixelBufferAccess
																									result)
{
	const tcu::ConstPixelBufferAccess
									expected							= m_expectedTextureLevel->getAccess();
	const tcu::UVec4				threshold							(0, 0, 0, 0);

	if (!tcu::intThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparsion", expected, result, threshold, tcu::COMPARE_LOG_RESULT))
	{
		return tcu::TestStatus::fail("Fill and Update Buffer test");
	}

	return tcu::TestStatus::pass("Fill and Update Buffer test");
}

void								FillBufferTestInstance::generateExpectedResult
																		(void)
{
	const tcu::ConstPixelBufferAccess
									dst									= m_destinationTextureLevel->getAccess();

	m_expectedTextureLevel	= de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(dst.getFormat(), dst.getWidth(), dst.getHeight(), dst.getDepth()));
	tcu::copy(m_expectedTextureLevel->getAccess(), dst);

	deUint32*						currentPtr							= (deUint32*) m_expectedTextureLevel->getAccess().getDataPtr() + m_params.dstOffset / 4;
	deUint32*						endPtr								= currentPtr + m_params.size / 4;

	while (currentPtr < endPtr)
	{
		*currentPtr = m_params.testData[0];
		currentPtr++;
	}
}

class FillBufferTestCase : public vkt::TestCase
{
public:
									FillBufferTestCase					(tcu::TestContext&			testCtx,
																		 const std::string&			name,
																		 const std::string&			description,
																		 const TestParams			params)
									: vkt::TestCase						(testCtx, name, description)
									, m_params							(params)
	{}

	virtual TestInstance*			createInstance						(Context&					context) const
	{
		return static_cast<TestInstance*>(new FillBufferTestInstance(context, m_params));
	}
private:
	const TestParams				m_params;
};

// Update Buffer

class UpdateBufferTestInstance : public FillBufferTestInstance
{
public:
									UpdateBufferTestInstance			(Context&					context,
																		 TestParams					testParams)
									: FillBufferTestInstance			(context, testParams)
	{}
	virtual tcu::TestStatus			iterate								(void);

protected:
	virtual void					generateExpectedResult				(void);
};

tcu::TestStatus						UpdateBufferTestInstance::iterate	(void)
{
	const int						dstLevelWidth						= (int)(m_params.dstSize / 4);
	m_destinationTextureLevel = de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(mapVkFormat(VK_FORMAT_R8G8B8A8_UINT), dstLevelWidth, 1));

	generateBuffer(m_destinationTextureLevel->getAccess(), dstLevelWidth, 1, 1);

	generateExpectedResult();

	uploadBuffer(m_destinationTextureLevel->getAccess(), *m_destinationBufferAlloc);

	const DeviceInterface&			vk									= m_context.getDeviceInterface();
	const VkDevice					vkDevice							= m_context.getDevice();
	const VkQueue					queue								= m_context.getUniversalQueue();

	const VkBufferMemoryBarrier		dstBufferBarrier					=
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,						// VkStructureType			sType;
		DE_NULL,														// const void*				pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,									// VkAccessFlags			srcAccessMask;
		VK_ACCESS_HOST_READ_BIT,										// VkAccessFlags			dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,										// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,										// deUint32					dstQueueFamilyIndex;
		*m_destination,													// VkBuffer					buffer;
		m_params.dstOffset,												// VkDeviceSize				offset;
		VK_WHOLE_SIZE												// VkDeviceSize				size;
	};

	beginCommandBuffer(vk, *m_cmdBuffer);
	vk.cmdUpdateBuffer(*m_cmdBuffer, *m_destination, m_params.dstOffset, m_params.size, m_params.testData);
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &dstBufferBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);
	endCommandBuffer(vk, *m_cmdBuffer);

	submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());

	// Read buffer data
	de::MovePtr<tcu::TextureLevel>	resultLevel	(new tcu::TextureLevel(m_destinationTextureLevel->getAccess().getFormat(), dstLevelWidth, 1));
	invalidateAlloc(vk, vkDevice, *m_destinationBufferAlloc);
	tcu::copy(*resultLevel, tcu::ConstPixelBufferAccess(resultLevel->getFormat(), resultLevel->getSize(), m_destinationBufferAlloc->getHostPtr()));

	return checkTestResult(resultLevel->getAccess());
}

void								UpdateBufferTestInstance::generateExpectedResult
																		(void)
{
	const tcu::ConstPixelBufferAccess
									dst									= m_destinationTextureLevel->getAccess();

	m_expectedTextureLevel	= de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(dst.getFormat(), dst.getWidth(), dst.getHeight(), dst.getDepth()));
	tcu::copy(m_expectedTextureLevel->getAccess(), dst);

	deUint32*						currentPtr							= (deUint32*) m_expectedTextureLevel->getAccess().getDataPtr() + m_params.dstOffset / 4;

	deMemcpy(currentPtr, m_params.testData, (size_t)m_params.size);
}

class UpdateBufferTestCase : public vkt::TestCase
{
public:
									UpdateBufferTestCase				(tcu::TestContext&			testCtx,
																		 const std::string&			name,
																		 const std::string&			description,
																		 const TestParams			params)
									: vkt::TestCase						(testCtx, name, description)
									, m_params							(params)
	{}

	virtual TestInstance*			createInstance						(Context&					context) const
	{
		return (TestInstance*) new UpdateBufferTestInstance(context, m_params);
	}
private:
	TestParams						m_params;
};

} // anonymous

tcu::TestCaseGroup*					createFillAndUpdateBufferTests	(tcu::TestContext&			testCtx)
{
	const de::SharedPtr<IBufferAllocator>
									bufferAllocators[]					=
	{
		de::SharedPtr<BufferSuballocation>(new BufferSuballocation()),
		de::SharedPtr<BufferDedicatedAllocation>(new BufferDedicatedAllocation())
	};

	de::MovePtr<tcu::TestCaseGroup>	fillAndUpdateBufferTests			(new tcu::TestCaseGroup(testCtx, "fill_and_update_buffer", "Fill and Update Buffer Tests"));
	tcu::TestCaseGroup*				bufferViewAllocationGroupTests[]
																		=
	{
		new tcu::TestCaseGroup(testCtx, "suballocation", "BufferView Fill and Update Tests for Suballocated Objects"),
		new tcu::TestCaseGroup(testCtx, "dedicated_alloc", "BufferView Fill and Update Tests for Dedicatedly Allocated Objects")
	};
	for (deUint32 subgroupNdx = 0u; subgroupNdx < DE_LENGTH_OF_ARRAY(bufferViewAllocationGroupTests); ++subgroupNdx)
	{
		if (bufferViewAllocationGroupTests[subgroupNdx] == DE_NULL)
		{
			TCU_THROW(InternalError, "Could not create test subgroup.");
		}
		fillAndUpdateBufferTests->addChild(bufferViewAllocationGroupTests[subgroupNdx]);
	}

	TestParams						params;

	for (deUint32 buffersAllocationNdx = 0u; buffersAllocationNdx < DE_LENGTH_OF_ARRAY(bufferAllocators); ++buffersAllocationNdx)
	{
		params.dstSize			= TestParams::TEST_DATA_SIZE;
		params.bufferAllocator	= bufferAllocators[buffersAllocationNdx];

		deUint8* data = (deUint8*) params.testData;
		for (deUint32 b = 0u; b < (params.dstSize * sizeof(params.testData[0])); b++)
			data[b] = (deUint8) (b % 255);
		const deUint32				testCaseGroupNdx					= buffersAllocationNdx;
		tcu::TestCaseGroup*			currentTestsGroup					= bufferViewAllocationGroupTests[testCaseGroupNdx];

		{
			const std::string		description							("whole buffer");
			const std::string		testName							("buffer_whole");

			params.dstOffset = 0;
			params.size = params.dstSize;

			currentTestsGroup->addChild(new FillBufferTestCase(testCtx, "fill_" + testName, "Fill " + description, params));
			currentTestsGroup->addChild(new UpdateBufferTestCase(testCtx, "update_" + testName, "Update " + description, params));
		}

		{
			const std::string		description							("first word in buffer");
			const std::string		testName							("buffer_first_one");

			params.dstOffset = 0;
			params.size = 4;

			currentTestsGroup->addChild(new FillBufferTestCase(testCtx, "fill_" + testName, "Fill " + description, params));
			currentTestsGroup->addChild(new UpdateBufferTestCase(testCtx, "update_" + testName, "Update " + description, params));
		}

		{
			const std::string		description							("second word in buffer");
			const std::string		testName							("buffer_second_one");

			params.dstOffset = 4;
			params.size = 4;

			currentTestsGroup->addChild(new FillBufferTestCase(testCtx, "fill_" + testName, "Fill " + description, params));
			currentTestsGroup->addChild(new UpdateBufferTestCase(testCtx, "update_" + testName, "Update " + description, params));
		}

		{
			const std::string		description							("buffer second part");
			const std::string		testName							("buffer_second_part");

			params.dstOffset = params.dstSize / 2;
			params.size = params.dstSize / 2;

			currentTestsGroup->addChild(new FillBufferTestCase(testCtx, "fill_" + testName, "Fill " + description, params));
			currentTestsGroup->addChild(new UpdateBufferTestCase(testCtx, "update_" + testName, "Update " + description, params));
		}

		// VK_WHOLE_SIZE tests.
		{
			for (VkDeviceSize i = 0; i < sizeof(deUint32); ++i)
			{
				for (VkDeviceSize j = 0; j < sizeof(deUint32); ++j)
				{
					params.dstSize		= TestParams::TEST_DATA_SIZE + i;
					params.dstOffset	= j * sizeof(deUint32);
					params.size			= VK_WHOLE_SIZE;

					const VkDeviceSize	extraBytes	= params.dstSize % sizeof(deUint32);
					const std::string	name		= "fill_buffer_vk_whole_size_" + de::toString(extraBytes) + "_extra_bytes_offset_" + de::toString(params.dstOffset);
					const std::string	description	= "vkCmdFillBuffer with VK_WHOLE_SIZE, " + de::toString(extraBytes) + " extra bytes and offset " + de::toString(params.dstOffset);

					currentTestsGroup->addChild(new FillWholeBufferTestCase{testCtx, name, description, params});
				}
			}
		}
	}

	return fillAndUpdateBufferTests.release();
}

} // api
} // vkt
