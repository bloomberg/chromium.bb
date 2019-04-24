// Copyright 2018 The SwiftShader Authors. All Rights Reserved.
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

#include "VkBuffer.hpp"
#include "VkConfig.h"
#include "VkDeviceMemory.hpp"

#include <cstring>

namespace vk
{

const size_t Buffer::DataOffset = offsetof(Buffer, memory);

Buffer::Buffer(const VkBufferCreateInfo* pCreateInfo, void* mem) :
	flags(pCreateInfo->flags), size(pCreateInfo->size), usage(pCreateInfo->usage),
	sharingMode(pCreateInfo->sharingMode), queueFamilyIndexCount(pCreateInfo->queueFamilyIndexCount),
	queueFamilyIndices(reinterpret_cast<uint32_t*>(mem))
{
	size_t queueFamilyIndicesSize = sizeof(uint32_t) * queueFamilyIndexCount;
	memcpy(queueFamilyIndices, pCreateInfo->pQueueFamilyIndices, queueFamilyIndicesSize);
}

void Buffer::destroy(const VkAllocationCallbacks* pAllocator)
{
	vk::deallocate(queueFamilyIndices, pAllocator);
}

size_t Buffer::ComputeRequiredAllocationSize(const VkBufferCreateInfo* pCreateInfo)
{
	return sizeof(uint32_t) * pCreateInfo->queueFamilyIndexCount;
}

const VkMemoryRequirements Buffer::getMemoryRequirements() const
{
	VkMemoryRequirements memoryRequirements = {};
	if(usage & (VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT))
	{
		memoryRequirements.alignment = vk::MIN_TEXEL_BUFFER_OFFSET_ALIGNMENT;
	}
	else if(usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
	{
		memoryRequirements.alignment = vk::MIN_STORAGE_BUFFER_OFFSET_ALIGNMENT;
	}
	else if(usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
	{
		memoryRequirements.alignment = vk::MIN_UNIFORM_BUFFER_OFFSET_ALIGNMENT;
	}
	else
	{
		memoryRequirements.alignment = REQUIRED_MEMORY_ALIGNMENT;
	}
	memoryRequirements.memoryTypeBits = vk::MEMORY_TYPE_GENERIC_BIT;
	memoryRequirements.size = size; // TODO: also reserve space for a header containing
		                            // the size of the buffer (for robust buffer access)
	return memoryRequirements;
}

void Buffer::bind(VkDeviceMemory pDeviceMemory, VkDeviceSize pMemoryOffset)
{
	memory = Cast(pDeviceMemory)->getOffsetPointer(pMemoryOffset);
}

void Buffer::copyFrom(const void* srcMemory, VkDeviceSize pSize, VkDeviceSize pOffset)
{
	ASSERT((pSize + pOffset) <= size);

	memcpy(getOffsetPointer(pOffset), srcMemory, pSize);
}

void Buffer::copyTo(void* dstMemory, VkDeviceSize pSize, VkDeviceSize pOffset) const
{
	ASSERT((pSize + pOffset) <= size);

	memcpy(dstMemory, getOffsetPointer(pOffset), pSize);
}

void Buffer::copyTo(Buffer* dstBuffer, const VkBufferCopy& pRegion) const
{
	copyTo(dstBuffer->getOffsetPointer(pRegion.dstOffset), pRegion.size, pRegion.srcOffset);
}

void Buffer::fill(VkDeviceSize dstOffset, VkDeviceSize fillSize, uint32_t data)
{
	ASSERT((fillSize + dstOffset) <= size);

	memset(getOffsetPointer(dstOffset), data, fillSize);
}

void Buffer::update(VkDeviceSize dstOffset, VkDeviceSize dataSize, const void* pData)
{
	ASSERT((dataSize + dstOffset) <= size);

	memcpy(getOffsetPointer(dstOffset), pData, dataSize);
}

void* Buffer::getOffsetPointer(VkDeviceSize offset) const
{
	return reinterpret_cast<char*>(memory) + offset;
}

} // namespace vk
