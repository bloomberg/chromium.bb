/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2018 The Khronos Group Inc.
 * Copyright (c) 2018 Google Inc.
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
 * \brief Utility for generating barriers
 *//*--------------------------------------------------------------------*/

#include "vkBarrierUtil.hpp"

namespace vk
{

VkBufferMemoryBarrier makeBufferMemoryBarrier (const VkAccessFlags	srcAccessMask,
											   const VkAccessFlags	dstAccessMask,
											   const VkBuffer		buffer,
											   const VkDeviceSize	offset,
											   const VkDeviceSize	bufferSizeBytes)
{
	const VkBufferMemoryBarrier barrier =
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType	sType;
		DE_NULL,									// const void*		pNext;
		srcAccessMask,								// VkAccessFlags	srcAccessMask;
		dstAccessMask,								// VkAccessFlags	dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			destQueueFamilyIndex;
		buffer,										// VkBuffer			buffer;
		offset,										// VkDeviceSize		offset;
		bufferSizeBytes,							// VkDeviceSize		size;
	};
	return barrier;
}

VkImageMemoryBarrier makeImageMemoryBarrier (const VkAccessFlags			srcAccessMask,
											 const VkAccessFlags			dstAccessMask,
											 const VkImageLayout			oldLayout,
											 const VkImageLayout			newLayout,
											 const VkImage					image,
											 const VkImageSubresourceRange	subresourceRange,
											 const deUint32					srcQueueFamilyIndex,
											 const deUint32					destQueueFamilyIndex)
{
	const VkImageMemoryBarrier barrier =
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,	// VkStructureType			sType;
		DE_NULL,								// const void*				pNext;
		srcAccessMask,							// VkAccessFlags			outputMask;
		dstAccessMask,							// VkAccessFlags			inputMask;
		oldLayout,								// VkImageLayout			oldLayout;
		newLayout,								// VkImageLayout			newLayout;
		srcQueueFamilyIndex,					// deUint32					srcQueueFamilyIndex;
		destQueueFamilyIndex,					// deUint32					destQueueFamilyIndex;
		image,									// VkImage					image;
		subresourceRange,						// VkImageSubresourceRange	subresourceRange;
	};
	return barrier;
}

VkMemoryBarrier makeMemoryBarrier (const VkAccessFlags	srcAccessMask,
								   const VkAccessFlags	dstAccessMask)
{
	const VkMemoryBarrier barrier =
	{
		VK_STRUCTURE_TYPE_MEMORY_BARRIER,	// VkStructureType    sType;
		DE_NULL,							// const void*        pNext;
		srcAccessMask,						// VkAccessFlags      srcAccessMask;
		dstAccessMask,						// VkAccessFlags      dstAccessMask;
	};
	return barrier;
}

} // vkt
