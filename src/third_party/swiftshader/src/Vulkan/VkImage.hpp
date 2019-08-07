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

#ifndef VK_IMAGE_HPP_
#define VK_IMAGE_HPP_

#include "VkObject.hpp"
#include "VkFormat.h"

namespace vk
{

class Device;
class DeviceMemory;

class Image : public Object<Image, VkImage>
{
public:
	struct CreateInfo
	{
		const VkImageCreateInfo* pCreateInfo;
		const VkDevice device;
	};

	Image(const CreateInfo* pCreateInfo, void* mem);
	~Image() = delete;
	void destroy(const VkAllocationCallbacks* pAllocator);

	static size_t ComputeRequiredAllocationSize(const CreateInfo* pCreateInfo);

	const VkMemoryRequirements getMemoryRequirements() const;
	void getSubresourceLayout(const VkImageSubresource* pSubresource, VkSubresourceLayout* pLayout) const;
	void bind(VkDeviceMemory pDeviceMemory, VkDeviceSize pMemoryOffset);
	void copyTo(VkImage dstImage, const VkImageCopy& pRegion);
	void copyTo(VkBuffer dstBuffer, const VkBufferImageCopy& region);
	void copyFrom(VkBuffer srcBuffer, const VkBufferImageCopy& region);

	void blit(VkImage dstImage, const VkImageBlit& region, VkFilter filter);
	void resolve(VkImage dstImage, const VkImageResolve& region);
	void clear(const VkClearValue& clearValue, const vk::Format& viewFormat, const VkRect2D& renderArea, const VkImageSubresourceRange& subresourceRange);
	void clear(const VkClearColorValue& color, const VkImageSubresourceRange& subresourceRange);
	void clear(const VkClearDepthStencilValue& color, const VkImageSubresourceRange& subresourceRange);

	VkImageType              getImageType() const { return imageType; }
	const Format&            getFormat() const { return format; }
	Format                   getFormat(VkImageAspectFlagBits aspect) const;
	uint32_t                 getArrayLayers() const { return arrayLayers; }
	uint32_t                 getMipLevels() const { return mipLevels; }
	VkImageUsageFlags        getUsage() const { return usage; }
	uint32_t                 getLastLayerIndex(const VkImageSubresourceRange& subresourceRange) const;
	uint32_t                 getLastMipLevel(const VkImageSubresourceRange& subresourceRange) const;
	VkSampleCountFlagBits    getSampleCountFlagBits() const { return samples; }
	VkExtent3D               getMipLevelExtent(uint32_t mipLevel) const;
	int                      rowPitchBytes(VkImageAspectFlagBits aspect, uint32_t mipLevel) const;
	int                      slicePitchBytes(VkImageAspectFlagBits aspect, uint32_t mipLevel) const;
	void*                    getTexelPointer(const VkOffset3D& offset, const VkImageSubresourceLayers& subresource) const;
	bool                     isCube() const;
	uint8_t*                 end() const;

	static Format            GetFormat(const vk::Format& format, VkImageAspectFlagBits aspect);

private:
	void copy(VkBuffer buffer, const VkBufferImageCopy& region, bool bufferIsSource);
	VkDeviceSize getStorageSize(VkImageAspectFlags flags) const;
	VkDeviceSize getMipLevelSize(VkImageAspectFlagBits aspect, uint32_t mipLevel) const;
	VkDeviceSize getMultiSampledLevelSize(VkImageAspectFlagBits aspect, uint32_t mipLevel) const;
	VkDeviceSize getLayerSize(VkImageAspectFlagBits aspect) const;
	VkDeviceSize getMemoryOffset(VkImageAspectFlagBits aspect, uint32_t mipLevel) const;
	VkDeviceSize getMemoryOffset(VkImageAspectFlagBits aspect, uint32_t mipLevel, uint32_t layer) const;
	VkDeviceSize texelOffsetBytesInStorage(const VkOffset3D& offset, const VkImageSubresourceLayers& subresource) const;
	VkDeviceSize getMemoryOffset(VkImageAspectFlagBits aspect) const;
	VkExtent3D imageExtentInBlocks(const VkExtent3D& extent, VkImageAspectFlagBits aspect) const;
	VkOffset3D imageOffsetInBlocks(const VkOffset3D& offset, VkImageAspectFlagBits aspect) const;
	VkExtent2D bufferExtentInBlocks(const VkExtent2D& extent, const VkBufferImageCopy& region) const;
	int bytesPerTexel(VkImageAspectFlagBits flags) const;
	VkFormat getClearFormat() const;
	void clear(void* pixelData, VkFormat pixelFormat, const vk::Format& viewFormat, const VkImageSubresourceRange& subresourceRange, const VkRect2D& renderArea);
	int borderSize(VkImageAspectFlagBits aspect) const;

	const Device *const      device = nullptr;
	DeviceMemory*            deviceMemory = nullptr;
	VkDeviceSize             memoryOffset = 0;
	VkImageCreateFlags       flags = 0;
	VkImageType              imageType = VK_IMAGE_TYPE_2D;
	Format                   format;
	VkExtent3D               extent = {0, 0, 0};
	uint32_t                 mipLevels = 0;
	uint32_t                 arrayLayers = 0;
	VkSampleCountFlagBits    samples = VK_SAMPLE_COUNT_1_BIT;
	VkImageTiling            tiling = VK_IMAGE_TILING_OPTIMAL;
	VkImageUsageFlags        usage = (VkImageUsageFlags)0;
};

static inline Image* Cast(VkImage object)
{
	return reinterpret_cast<Image*>(object);
}

} // namespace vk

#endif // VK_IMAGE_HPP_