#ifndef _VKTCOMPUTETESTSUTIL_HPP
#define _VKTCOMPUTETESTSUTIL_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \brief Compute tests utility classes
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "vkMemUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkPrograms.hpp"
#include "vkTypeUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkObjUtil.hpp"

namespace vkt
{
namespace compute
{

class Buffer
{
public:
									Buffer			(const vk::DeviceInterface&		vk,
													 const vk::VkDevice				device,
													 vk::Allocator&					allocator,
													 const vk::VkBufferCreateInfo&	bufferCreateInfo,
													 const vk::MemoryRequirement	memoryRequirement);

	const vk::VkBuffer&				get				(void) const { return *m_buffer; }
	const vk::VkBuffer&				operator*		(void) const { return get(); }
	vk::Allocation&					getAllocation	(void) const { return *m_allocation; }

private:
	de::MovePtr<vk::Allocation>		m_allocation;
	vk::Move<vk::VkBuffer>			m_buffer;

									Buffer			(const Buffer&);  // "deleted"
	Buffer&							operator=		(const Buffer&);
};

class Image
{
public:
									Image			(const vk::DeviceInterface&		vk,
													 const vk::VkDevice				device,
													 vk::Allocator&					allocator,
													 const vk::VkImageCreateInfo&	imageCreateInfo,
													 const vk::MemoryRequirement	memoryRequirement);

	const vk::VkImage&				get				(void) const { return *m_image; }
	const vk::VkImage&				operator*		(void) const { return get(); }
	vk::Allocation&					getAllocation	(void) const { return *m_allocation; }

private:
	de::MovePtr<vk::Allocation>		m_allocation;
	vk::Move<vk::VkImage>			m_image;

									Image			(const Image&);  // "deleted"
	Image&							operator=		(const Image&);
};

vk::Move<vk::VkPipeline>		makeComputePipeline				(const vk::DeviceInterface&					vk,
																 const vk::VkDevice							device,
																 const vk::VkPipelineLayout					pipelineLayout,
																 const vk::VkShaderModule					shaderModule);

vk::Move<vk::VkPipeline>		makeComputePipeline				(const vk::DeviceInterface&					vk,
																 const vk::VkDevice							device,
																 const vk::VkPipelineLayout					pipelineLayout,
																 const vk::VkPipelineCreateFlags			pipelineFlags,
																 const vk::VkShaderModule					shaderModule,
																 const vk::VkPipelineShaderStageCreateFlags	shaderFlags);

vk::VkBufferImageCopy			makeBufferImageCopy				(const vk::VkExtent3D						extent,
																 const deUint32								arraySize);

inline vk::VkExtent3D makeExtent3D (const tcu::IVec3& vec)
{
	return vk::makeExtent3D(vec.x(), vec.y(), vec.z());
}

inline vk::VkDeviceSize getImageSizeBytes (const tcu::IVec3& imageSize, const vk::VkFormat format)
{
	return tcu::getPixelSize(vk::mapVkFormat(format)) * imageSize.x() * imageSize.y() * imageSize.z();
}

} // compute
} // vkt

#endif // _VKTCOMPUTETESTSUTIL_HPP
