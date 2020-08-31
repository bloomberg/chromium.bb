/*------------------------------------------------------------------------
 *  Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
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
 * \brief Vulkan Buffers Tests
 *//*--------------------------------------------------------------------*/

#include "vktApiBufferTests.hpp"
#include "gluVarType.hpp"
#include "deStringUtil.hpp"
#include "tcuTestLog.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "tcuPlatform.hpp"

#include <algorithm>

namespace vkt
{
namespace api
{
namespace
{
using namespace vk;

enum AllocationKind
{
	ALLOCATION_KIND_SUBALLOCATED = 0,
	ALLOCATION_KIND_DEDICATED,

	ALLOCATION_KIND_LAST,
};

PlatformMemoryLimits getPlatformMemoryLimits (Context& context)
{
	PlatformMemoryLimits	memoryLimits;

	context.getTestContext().getPlatform().getVulkanPlatform().getMemoryLimits(memoryLimits);

	return memoryLimits;
}

VkDeviceSize getMaxBufferSize(const VkDeviceSize& bufferSize,
							  const VkDeviceSize& alignment,
							  const PlatformMemoryLimits& limits)
{
	VkDeviceSize size = bufferSize;

	if (limits.totalDeviceLocalMemory == 0)
	{
		// 'UMA' systems where device memory counts against system memory
		size = std::min(bufferSize, limits.totalSystemMemory - alignment);
	}
	else
	{
		// 'LMA' systems where device memory is local to the GPU
		size = std::min(bufferSize, limits.totalDeviceLocalMemory - alignment);
	}

	return size;
}

struct BufferCaseParameters
{
	VkBufferUsageFlags	usage;
	VkBufferCreateFlags	flags;
	VkSharingMode		sharingMode;
};

class BufferTestInstance : public TestInstance
{
public:
										BufferTestInstance				(Context&					ctx,
																		 BufferCaseParameters		testCase)
										: TestInstance					(ctx)
										, m_testCase					(testCase)
	{
	}
	virtual tcu::TestStatus				iterate							(void);
	virtual tcu::TestStatus				bufferCreateAndAllocTest		(VkDeviceSize				size);

protected:
	BufferCaseParameters				m_testCase;
};

class DedicatedAllocationBufferTestInstance : public BufferTestInstance
{
public:
										DedicatedAllocationBufferTestInstance
																		(Context&					ctx,
																		 BufferCaseParameters		testCase)
										: BufferTestInstance			(ctx, testCase)
	{
	}
	virtual tcu::TestStatus				bufferCreateAndAllocTest		(VkDeviceSize				size);
};

class BuffersTestCase : public TestCase
{
public:
										BuffersTestCase					(tcu::TestContext&			testCtx,
																		 const std::string&			name,
																		 const std::string&			description,
																		 BufferCaseParameters		testCase)
										: TestCase						(testCtx, name, description)
										, m_testCase					(testCase)
	{
	}

	virtual								~BuffersTestCase				(void)
	{
	}

	virtual TestInstance*				createInstance					(Context&					ctx) const
	{
		tcu::TestLog&					log								= m_testCtx.getLog();
		log << tcu::TestLog::Message << getBufferUsageFlagsStr(m_testCase.usage) << tcu::TestLog::EndMessage;
		return new BufferTestInstance(ctx, m_testCase);
	}

	virtual void						checkSupport					(Context&					ctx) const
	{
		const VkPhysicalDeviceFeatures&		physicalDeviceFeatures = getPhysicalDeviceFeatures(ctx.getInstanceInterface(), ctx.getPhysicalDevice());

		if ((m_testCase.flags & VK_BUFFER_CREATE_SPARSE_BINDING_BIT) && !physicalDeviceFeatures.sparseBinding)
			TCU_THROW(NotSupportedError, "Sparse bindings feature is not supported");

		if ((m_testCase.flags & VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT) && !physicalDeviceFeatures.sparseResidencyBuffer)
			TCU_THROW(NotSupportedError, "Sparse buffer residency feature is not supported");

		if ((m_testCase.flags & VK_BUFFER_CREATE_SPARSE_ALIASED_BIT) && !physicalDeviceFeatures.sparseResidencyAliased)
			TCU_THROW(NotSupportedError, "Sparse aliased residency feature is not supported");
	}

private:
	BufferCaseParameters				m_testCase;
};

class DedicatedAllocationBuffersTestCase : public TestCase
{
	public:
										DedicatedAllocationBuffersTestCase
																		(tcu::TestContext&			testCtx,
																		 const std::string&			name,
																		 const std::string&			description,
																		 BufferCaseParameters		testCase)
										: TestCase						(testCtx, name, description)
										, m_testCase					(testCase)
	{
	}

	virtual								~DedicatedAllocationBuffersTestCase
																		(void)
	{
	}

	virtual TestInstance*				createInstance					(Context&					ctx) const
	{
		tcu::TestLog&					log								= m_testCtx.getLog();
		log << tcu::TestLog::Message << getBufferUsageFlagsStr(m_testCase.usage) << tcu::TestLog::EndMessage;
		return new DedicatedAllocationBufferTestInstance(ctx, m_testCase);
	}

	virtual void						checkSupport					(Context&					ctx) const
	{
		if (!ctx.isDeviceFunctionalitySupported("VK_KHR_dedicated_allocation"))
			TCU_THROW(NotSupportedError, "Not supported");
	}
private:
	BufferCaseParameters				m_testCase;
};

tcu::TestStatus BufferTestInstance::bufferCreateAndAllocTest			(VkDeviceSize				size)
{
	const VkPhysicalDevice				vkPhysicalDevice				= m_context.getPhysicalDevice();
	const InstanceInterface&			vkInstance						= m_context.getInstanceInterface();
	const VkDevice						vkDevice						= m_context.getDevice();
	const DeviceInterface&				vk								= m_context.getDeviceInterface();
	const deUint32						queueFamilyIndex				= m_context.getSparseQueueFamilyIndex();
	const VkPhysicalDeviceMemoryProperties
										memoryProperties				= getPhysicalDeviceMemoryProperties(vkInstance, vkPhysicalDevice);
	const VkPhysicalDeviceLimits		limits							= getPhysicalDeviceProperties(vkInstance, vkPhysicalDevice).limits;
	Move<VkBuffer>						buffer;
	Move<VkDeviceMemory>				memory;
	VkMemoryRequirements				memReqs;

	if ((m_testCase.flags & VK_BUFFER_CREATE_SPARSE_BINDING_BIT) != 0)
	{
		size = std::min(size, limits.sparseAddressSpaceSize);
	}

	// Create the test buffer and a memory allocation for it
	{
		// Create a minimal buffer first to get the supported memory types
		VkBufferCreateInfo				bufferParams					=
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,						// VkStructureType			sType;
			DE_NULL,													// const void*				pNext;
			m_testCase.flags,											// VkBufferCreateFlags		flags;
			1u,															// VkDeviceSize				size;
			m_testCase.usage,											// VkBufferUsageFlags		usage;
			m_testCase.sharingMode,										// VkSharingMode			sharingMode;
			1u,															// uint32_t					queueFamilyIndexCount;
			&queueFamilyIndex,											// const uint32_t*			pQueueFamilyIndices;
		};

		buffer = createBuffer(vk, vkDevice, &bufferParams);
		vk.getBufferMemoryRequirements(vkDevice, *buffer, &memReqs);

		const deUint32					heapTypeIndex					= (deUint32)deCtz32(memReqs.memoryTypeBits);
		const VkMemoryType				memoryType						= memoryProperties.memoryTypes[heapTypeIndex];
		const VkMemoryHeap				memoryHeap						= memoryProperties.memoryHeaps[memoryType.heapIndex];
		const deUint32					shrinkBits						= 4u;	// number of bits to shift when reducing the size with each iteration

		// Buffer size - Choose half of the reported heap size for the maximum buffer size, we
		// should attempt to test as large a portion as possible.
		//
		// However on a system where device memory is shared with the system, the maximum size
		// should be tested against the platform memory limits as significant portion of the heap
		// may already be in use by the operating system and other running processes.
		const VkDeviceSize  availableBufferSize	= getMaxBufferSize(memoryHeap.size,
																   memReqs.alignment,
																   getPlatformMemoryLimits(m_context));

		// For our test buffer size, halve the maximum available size and align
		const VkDeviceSize maxBufferSize = deAlign64(availableBufferSize >> 1, memReqs.alignment);

		size = std::min(size, maxBufferSize);

		while (*memory == DE_NULL)
		{
			// Create the buffer
			{
				VkResult				result							= VK_ERROR_OUT_OF_HOST_MEMORY;
				VkBuffer				rawBuffer						= DE_NULL;

				bufferParams.size = size;
				buffer = Move<VkBuffer>();		// free the previous buffer, if any
				result = vk.createBuffer(vkDevice, &bufferParams, (vk::VkAllocationCallbacks*)DE_NULL, &rawBuffer);

				if (result != VK_SUCCESS)
				{
					size = deAlign64(size >> shrinkBits, memReqs.alignment);

					if (size == 0 || bufferParams.size == memReqs.alignment)
					{
						return tcu::TestStatus::fail("Buffer creation failed! (" + de::toString(getResultName(result)) + ")");
					}

					continue;	// didn't work, try with a smaller buffer
				}

				buffer = Move<VkBuffer>(check<VkBuffer>(rawBuffer), Deleter<VkBuffer>(vk, vkDevice, DE_NULL));
			}

			vk.getBufferMemoryRequirements(vkDevice, *buffer, &memReqs);	// get the proper size requirement

			if (size > memReqs.size)
			{
				std::ostringstream		errorMsg;
				errorMsg << "Requied memory size (" << memReqs.size << " bytes) smaller than the buffer's size (" << size << " bytes)!";
				return tcu::TestStatus::fail(errorMsg.str());
			}

			// Allocate the memory
			{
				VkResult				result							= VK_ERROR_OUT_OF_HOST_MEMORY;
				VkDeviceMemory			rawMemory						= DE_NULL;

				const VkMemoryAllocateInfo
										memAlloc						=
				{
					VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,				// VkStructureType			sType;
					NULL,												// const void*				pNext;
					memReqs.size,										// VkDeviceSize				allocationSize;
					heapTypeIndex,										// uint32_t					memoryTypeIndex;
				};

				result = vk.allocateMemory(vkDevice, &memAlloc, (VkAllocationCallbacks*)DE_NULL, &rawMemory);

				if (result != VK_SUCCESS)
				{
					size = deAlign64(size >> shrinkBits, memReqs.alignment);

					if (size == 0 || memReqs.size == memReqs.alignment)
					{
						return tcu::TestStatus::fail("Unable to allocate " + de::toString(memReqs.size) + " bytes of memory");
					}

					continue;	// didn't work, try with a smaller allocation (and a smaller buffer)
				}

				memory = Move<VkDeviceMemory>(check<VkDeviceMemory>(rawMemory), Deleter<VkDeviceMemory>(vk, vkDevice, DE_NULL));
			}
		} // while
	}

	// Bind the memory
	if ((m_testCase.flags & (VK_BUFFER_CREATE_SPARSE_BINDING_BIT | VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT | VK_BUFFER_CREATE_SPARSE_ALIASED_BIT)) != 0)
	{
		const VkQueue					queue							= m_context.getSparseQueue();

		const VkSparseMemoryBind		sparseMemoryBind				=
		{
			0,															// VkDeviceSize				resourceOffset;
			memReqs.size,												// VkDeviceSize				size;
			*memory,													// VkDeviceMemory			memory;
			0,															// VkDeviceSize				memoryOffset;
			0															// VkSparseMemoryBindFlags	flags;
		};

		const VkSparseBufferMemoryBindInfo
										sparseBufferMemoryBindInfo		=
		{
			*buffer,													// VkBuffer					buffer;
			1u,															// deUint32					bindCount;
			&sparseMemoryBind											// const VkSparseMemoryBind* pBinds;
		};

		const VkBindSparseInfo			bindSparseInfo					=
		{
			VK_STRUCTURE_TYPE_BIND_SPARSE_INFO,							// VkStructureType			sType;
			DE_NULL,													// const void*				pNext;
			0,															// deUint32					waitSemaphoreCount;
			DE_NULL,													// const VkSemaphore*		pWaitSemaphores;
			1u,															// deUint32					bufferBindCount;
			&sparseBufferMemoryBindInfo,								// const VkSparseBufferMemoryBindInfo* pBufferBinds;
			0,															// deUint32					imageOpaqueBindCount;
			DE_NULL,													// const VkSparseImageOpaqueMemoryBindInfo*	pImageOpaqueBinds;
			0,															// deUint32					imageBindCount;
			DE_NULL,													// const VkSparseImageMemoryBindInfo* pImageBinds;
			0,															// deUint32					signalSemaphoreCount;
			DE_NULL,													// const VkSemaphore*		pSignalSemaphores;
		};

		const vk::Unique<vk::VkFence>	fence							(vk::createFence(vk, vkDevice));

		if (vk.queueBindSparse(queue, 1, &bindSparseInfo, *fence) != VK_SUCCESS)
			return tcu::TestStatus::fail("Bind sparse buffer memory failed! (requested memory size: " + de::toString(size) + ")");

		VK_CHECK(vk.waitForFences(vkDevice, 1, &fence.get(), VK_TRUE, ~(0ull) /* infinity */));
	}
	else if (vk.bindBufferMemory(vkDevice, *buffer, *memory, 0) != VK_SUCCESS)
		return tcu::TestStatus::fail("Bind buffer memory failed! (requested memory size: " + de::toString(size) + ")");

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus							BufferTestInstance::iterate		(void)
{
	const VkDeviceSize					testSizes[]						=
	{
		1,
		1181,
		15991,
		16384,
		~0ull,		// try to exercise a very large buffer too (will be clamped to a sensible size later)
	};

	for (int i = 0; i < DE_LENGTH_OF_ARRAY(testSizes); ++i)
	{
		const tcu::TestStatus			testStatus						= bufferCreateAndAllocTest(testSizes[i]);

		if (testStatus.getCode() != QP_TEST_RESULT_PASS)
			return testStatus;
	}

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus							DedicatedAllocationBufferTestInstance::bufferCreateAndAllocTest
																		(VkDeviceSize				size)
{
	const VkPhysicalDevice				vkPhysicalDevice				= m_context.getPhysicalDevice();
	const InstanceInterface&			vkInstance						= m_context.getInstanceInterface();
	const VkDevice						vkDevice						= m_context.getDevice();
	const DeviceInterface&				vk								= m_context.getDeviceInterface();
	const deUint32						queueFamilyIndex				= m_context.getUniversalQueueFamilyIndex();
	const VkPhysicalDeviceMemoryProperties
										memoryProperties				= getPhysicalDeviceMemoryProperties(vkInstance, vkPhysicalDevice);
	const VkPhysicalDeviceLimits		limits							= getPhysicalDeviceProperties(vkInstance, vkPhysicalDevice).limits;

	VkMemoryDedicatedRequirements	dedicatedRequirements			=
	{
		VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS,				// VkStructureType			sType;
		DE_NULL,														// const void*				pNext;
		false,															// VkBool32					prefersDedicatedAllocation
		false															// VkBool32					requiresDedicatedAllocation
	};
	VkMemoryRequirements2			memReqs							=
	{
		VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,						// VkStructureType			sType
		&dedicatedRequirements,											// void*					pNext
		{0, 0, 0}														// VkMemoryRequirements		memoryRequirements
	};

	if ((m_testCase.flags & VK_BUFFER_CREATE_SPARSE_BINDING_BIT) != 0)
		size = std::min(size, limits.sparseAddressSpaceSize);

	// Create a minimal buffer first to get the supported memory types
	VkBufferCreateInfo					bufferParams					=
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,							// VkStructureType			sType
		DE_NULL,														// const void*				pNext
		m_testCase.flags,												// VkBufferCreateFlags		flags
		1u,																// VkDeviceSize				size
		m_testCase.usage,												// VkBufferUsageFlags		usage
		m_testCase.sharingMode,											// VkSharingMode			sharingMode
		1u,																// uint32_t					queueFamilyIndexCount
		&queueFamilyIndex,												// const uint32_t*			pQueueFamilyIndices
	};

	Move<VkBuffer>						buffer							= createBuffer(vk, vkDevice, &bufferParams);

	VkBufferMemoryRequirementsInfo2	info							=
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2,			// VkStructureType			sType
		DE_NULL,														// const void*				pNext
		*buffer															// VkBuffer					buffer
	};

	vk.getBufferMemoryRequirements2(vkDevice, &info, &memReqs);

	if (dedicatedRequirements.requiresDedicatedAllocation == VK_TRUE)
	{
		std::ostringstream				errorMsg;
		errorMsg << "Nonexternal objects cannot require dedicated allocation.";
		return tcu::TestStatus::fail(errorMsg.str());
	}

	const deUint32						heapTypeIndex					= static_cast<deUint32>(deCtz32(memReqs.memoryRequirements.memoryTypeBits));
	const VkMemoryType					memoryType						= memoryProperties.memoryTypes[heapTypeIndex];
	const VkMemoryHeap					memoryHeap						= memoryProperties.memoryHeaps[memoryType.heapIndex];
	const deUint32						shrinkBits						= 4u;	// number of bits to shift when reducing the size with each iteration

	// Buffer size - Choose half of the reported heap size for the maximum buffer size, we
	// should attempt to test as large a portion as possible.
	//
	// However on a system where device memory is shared with the system, the maximum size
	// should be tested against the platform memory limits as a significant portion of the heap
	// may already be in use by the operating system and other running processes.
	const VkDeviceSize maxBufferSize = getMaxBufferSize(memoryHeap.size,
													   memReqs.memoryRequirements.alignment,
													   getPlatformMemoryLimits(m_context));

	Move<VkDeviceMemory>				memory;
	size = deAlign64(std::min(size, maxBufferSize >> 1), memReqs.memoryRequirements.alignment);
	while (*memory == DE_NULL)
	{
		// Create the buffer
		{
			VkResult					result							= VK_ERROR_OUT_OF_HOST_MEMORY;
			VkBuffer					rawBuffer						= DE_NULL;

			bufferParams.size = size;
			buffer = Move<VkBuffer>(); // free the previous buffer, if any
			result = vk.createBuffer(vkDevice, &bufferParams, (VkAllocationCallbacks*)DE_NULL, &rawBuffer);

			if (result != VK_SUCCESS)
			{
				size = deAlign64(size >> shrinkBits, memReqs.memoryRequirements.alignment);

				if (size == 0 || bufferParams.size == memReqs.memoryRequirements.alignment)
					return tcu::TestStatus::fail("Buffer creation failed! (" + de::toString(getResultName(result)) + ")");

				continue; // didn't work, try with a smaller buffer
			}

			buffer = Move<VkBuffer>(check<VkBuffer>(rawBuffer), Deleter<VkBuffer>(vk, vkDevice, DE_NULL));
		}

		info.buffer = *buffer;
		vk.getBufferMemoryRequirements2(vkDevice, &info, &memReqs);		// get the proper size requirement

		if (size > memReqs.memoryRequirements.size)
		{
			std::ostringstream			errorMsg;
			errorMsg << "Requied memory size (" << memReqs.memoryRequirements.size << " bytes) smaller than the buffer's size (" << size << " bytes)!";
			return tcu::TestStatus::fail(errorMsg.str());
		}

		// Allocate the memory
		{
			VkResult					result							= VK_ERROR_OUT_OF_HOST_MEMORY;
			VkDeviceMemory				rawMemory						= DE_NULL;

			vk::VkMemoryDedicatedAllocateInfo
										dedicatedInfo					=
			{
				VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,		// VkStructureType			sType
				DE_NULL,												// const void*				pNext
				DE_NULL,												// VkImage					image
				*buffer													// VkBuffer					buffer
			};

			VkMemoryAllocateInfo		memoryAllocateInfo				=
			{
				VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,					// VkStructureType			sType
				&dedicatedInfo,											// const void*				pNext
				memReqs.memoryRequirements.size,						// VkDeviceSize				allocationSize
				heapTypeIndex,											// deUint32					memoryTypeIndex
			};

			result = vk.allocateMemory(vkDevice, &memoryAllocateInfo, (VkAllocationCallbacks*)DE_NULL, &rawMemory);

			if (result != VK_SUCCESS)
			{
				size = deAlign64(size >> shrinkBits, memReqs.memoryRequirements.alignment);

				if (size == 0 || memReqs.memoryRequirements.size == memReqs.memoryRequirements.alignment)
					return tcu::TestStatus::fail("Unable to allocate " + de::toString(memReqs.memoryRequirements.size) + " bytes of memory");

				continue; // didn't work, try with a smaller allocation (and a smaller buffer)
			}

			memory = Move<VkDeviceMemory>(check<VkDeviceMemory>(rawMemory), Deleter<VkDeviceMemory>(vk, vkDevice, DE_NULL));
		}
	} // while

	if (vk.bindBufferMemory(vkDevice, *buffer, *memory, 0) != VK_SUCCESS)
		return tcu::TestStatus::fail("Bind buffer memory failed! (requested memory size: " + de::toString(size) + ")");

	return tcu::TestStatus::pass("Pass");
}

std::string getBufferUsageFlagsName (const VkBufferUsageFlags flags)
{
	switch (flags)
	{
		case VK_BUFFER_USAGE_TRANSFER_SRC_BIT:			return "transfer_src";
		case VK_BUFFER_USAGE_TRANSFER_DST_BIT:			return "transfer_dst";
		case VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT:	return "uniform_texel";
		case VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT:	return "storage_texel";
		case VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT:		return "uniform";
		case VK_BUFFER_USAGE_STORAGE_BUFFER_BIT:		return "storage";
		case VK_BUFFER_USAGE_INDEX_BUFFER_BIT:			return "index";
		case VK_BUFFER_USAGE_VERTEX_BUFFER_BIT:			return "vertex";
		case VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT:		return "indirect";
		default:
			DE_FATAL("Unknown buffer usage flag");
			return "";
	}
}

std::string getBufferCreateFlagsName (const VkBufferCreateFlags flags)
{
	std::ostringstream name;

	if (flags & VK_BUFFER_CREATE_SPARSE_BINDING_BIT)
		name << "_binding";
	if (flags & VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT)
		name << "_residency";
	if (flags & VK_BUFFER_CREATE_SPARSE_ALIASED_BIT)
		name << "_aliased";
	if (flags == 0u)
		name << "_zero";

	DE_ASSERT(!name.str().empty());

	return name.str().substr(1);
}

// Create all VkBufferUsageFlags combinations recursively
void createBufferUsageCases (tcu::TestCaseGroup& testGroup, const deUint32 firstNdx, const deUint32 bufferUsageFlags, const AllocationKind allocationKind)
{
	const VkBufferUsageFlags			bufferUsageModes[]	=
	{
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT,
		VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
	};

	tcu::TestContext&					testCtx				= testGroup.getTestContext();

	// Add test groups
	for (deUint32 currentNdx = firstNdx; currentNdx < DE_LENGTH_OF_ARRAY(bufferUsageModes); currentNdx++)
	{
		const deUint32					newBufferUsageFlags	= bufferUsageFlags | bufferUsageModes[currentNdx];
		const std::string				newGroupName		= getBufferUsageFlagsName(bufferUsageModes[currentNdx]);
		de::MovePtr<tcu::TestCaseGroup>	newTestGroup		(new tcu::TestCaseGroup(testCtx, newGroupName.c_str(), ""));

		createBufferUsageCases(*newTestGroup, currentNdx + 1u, newBufferUsageFlags, allocationKind);
		testGroup.addChild(newTestGroup.release());
	}

	// Add test cases
	if (bufferUsageFlags != 0u)
	{
		// \note SPARSE_RESIDENCY and SPARSE_ALIASED have to be used together with the SPARSE_BINDING flag.
		const VkBufferCreateFlags		bufferCreateFlags[]		=
		{
			0,
			VK_BUFFER_CREATE_SPARSE_BINDING_BIT,
			VK_BUFFER_CREATE_SPARSE_BINDING_BIT | VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT,
			VK_BUFFER_CREATE_SPARSE_BINDING_BIT | VK_BUFFER_CREATE_SPARSE_ALIASED_BIT,
			VK_BUFFER_CREATE_SPARSE_BINDING_BIT | VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT | VK_BUFFER_CREATE_SPARSE_ALIASED_BIT,
		};

		// Dedicated allocation does not support sparse feature
		const int						numBufferCreateFlags	= (allocationKind == ALLOCATION_KIND_SUBALLOCATED) ? DE_LENGTH_OF_ARRAY(bufferCreateFlags) : 1;

		de::MovePtr<tcu::TestCaseGroup>	newTestGroup			(new tcu::TestCaseGroup(testCtx, "create", ""));

		for (int bufferCreateFlagsNdx = 0; bufferCreateFlagsNdx < numBufferCreateFlags; bufferCreateFlagsNdx++)
		{
			const BufferCaseParameters	testParams	=
			{
				bufferUsageFlags,
				bufferCreateFlags[bufferCreateFlagsNdx],
				VK_SHARING_MODE_EXCLUSIVE
			};

			const std::string			allocStr	= (allocationKind == ALLOCATION_KIND_SUBALLOCATED) ? "suballocation of " : "dedicated alloc. of ";
			const std::string			caseName	= getBufferCreateFlagsName(bufferCreateFlags[bufferCreateFlagsNdx]);
			const std::string			caseDesc	= "vkCreateBuffer test: " + allocStr + de::toString(bufferUsageFlags) + " " + de::toString(testParams.flags);

			switch (allocationKind)
			{
				case ALLOCATION_KIND_SUBALLOCATED:
					newTestGroup->addChild(new BuffersTestCase(testCtx, caseName.c_str(), caseDesc.c_str(), testParams));
					break;
				case ALLOCATION_KIND_DEDICATED:
					newTestGroup->addChild(new DedicatedAllocationBuffersTestCase(testCtx, caseName.c_str(), caseDesc.c_str(), testParams));
					break;
				default:
					DE_FATAL("Unknown test type");
			}
		}
		testGroup.addChild(newTestGroup.release());
	}
}

tcu::TestStatus testOverlyLargeBuffer(Context& context, deUint64 bufferSize)
{
	const DeviceInterface&	vk					= context.getDeviceInterface();
	const VkDevice			vkDevice			= context.getDevice();
	const deUint32			queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
	VkBuffer				rawBuffer			= DE_NULL;

	VkBufferCreateInfo bufferParams =
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType			sType;
		DE_NULL,								// const void*				pNext;
		0u,										// VkBufferCreateFlags		flags;
		bufferSize,								// VkDeviceSize				size;
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,		// VkBufferUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode;
		1u,										// uint32_t					queueFamilyIndexCount;
		&queueFamilyIndex,						// const uint32_t*			pQueueFamilyIndices;
	};

	VkResult result = vk.createBuffer(vkDevice, &bufferParams, (vk::VkAllocationCallbacks*)DE_NULL, &rawBuffer);

	// if buffer creation succeeds verify that the correct amount of memory was bound to it
	if (result == VK_SUCCESS)
	{
		VkMemoryRequirements memoryRequirements;
		vk.getBufferMemoryRequirements(vkDevice, rawBuffer, &memoryRequirements);
		vk.destroyBuffer(vkDevice, rawBuffer, DE_NULL);

		if (memoryRequirements.size >= bufferSize)
			return tcu::TestStatus::pass("Pass");
		return tcu::TestStatus::fail("Fail");
	}

	vk.destroyBuffer(vkDevice, rawBuffer, DE_NULL);

	// check if one of the allowed errors was returned
	if ((result == VK_ERROR_OUT_OF_DEVICE_MEMORY) ||
		(result == VK_ERROR_OUT_OF_HOST_MEMORY))
		return tcu::TestStatus::pass("Pass");

	return tcu::TestStatus::fail("Fail");
}

} // anonymous

 tcu::TestCaseGroup* createBufferTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> buffersTests (new tcu::TestCaseGroup(testCtx, "buffer", "Buffer Tests"));

	{
		de::MovePtr<tcu::TestCaseGroup>	regularAllocation	(new tcu::TestCaseGroup(testCtx, "suballocation", "Regular suballocation of memory."));
		createBufferUsageCases(*regularAllocation, 0u, 0u, ALLOCATION_KIND_SUBALLOCATED);
		buffersTests->addChild(regularAllocation.release());
	}

	{
		de::MovePtr<tcu::TestCaseGroup>	dedicatedAllocation	(new tcu::TestCaseGroup(testCtx, "dedicated_alloc", "Dedicated allocation of memory."));
		createBufferUsageCases(*dedicatedAllocation, 0u, 0u, ALLOCATION_KIND_DEDICATED);
		buffersTests->addChild(dedicatedAllocation.release());
	}

	{
		de::MovePtr<tcu::TestCaseGroup> basicTests(new tcu::TestCaseGroup(testCtx, "basic", "Basic buffer tests."));
		addFunctionCase(basicTests.get(), "size_max_uint64", "Creating a ULLONG_MAX buffer and verify that it either succeeds or returns one of the allowed errors.", testOverlyLargeBuffer, std::numeric_limits<deUint64>::max());
		buffersTests->addChild(basicTests.release());
	}

	return buffersTests.release();
}

} // api
} // vk
