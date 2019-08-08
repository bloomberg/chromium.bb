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

#ifndef VK_IMAGE_VIEW_HPP_
#define VK_IMAGE_VIEW_HPP_

#include "VkDebug.hpp"
#include "VkFormat.h"
#include "VkObject.hpp"
#include "VkImage.hpp"

namespace vk
{

class ImageView : public Object<ImageView, VkImageView>
{
public:
	ImageView(const VkImageViewCreateInfo* pCreateInfo, void* mem);
	~ImageView() = delete;
	void destroy(const VkAllocationCallbacks* pAllocator);

	static size_t ComputeRequiredAllocationSize(const VkImageViewCreateInfo* pCreateInfo);

	void clear(const VkClearValue& clearValues, VkImageAspectFlags aspectMask, const VkRect2D& renderArea);
	void clear(const VkClearValue& clearValue, VkImageAspectFlags aspectMask, const VkClearRect& renderArea);
	void resolve(ImageView* resolveAttachment);

	VkImageViewType getType() const { return viewType; }
	Format getFormat() const { return format; }
	int getSampleCount() const { return image->getSampleCountFlagBits(); }
	int rowPitchBytes(VkImageAspectFlagBits aspect, uint32_t mipLevel) const { return image->rowPitchBytes(aspect, subresourceRange.baseMipLevel + mipLevel); }
	int slicePitchBytes(VkImageAspectFlagBits aspect, uint32_t mipLevel) const { return image->slicePitchBytes(aspect, subresourceRange.baseMipLevel + mipLevel); }
	VkExtent3D getMipLevelExtent(uint32_t mipLevel) const { return image->getMipLevelExtent(subresourceRange.baseMipLevel + mipLevel); }

	void *getOffsetPointer(const VkOffset3D& offset, VkImageAspectFlagBits aspect) const;
	bool hasDepthAspect() const { return (subresourceRange.aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT) != 0; }
	bool hasStencilAspect() const { return (subresourceRange.aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT) != 0; }

	const VkComponentMapping &getComponentMapping() const { return components; }
	const VkImageSubresourceRange &getSubresourceRange() const { return subresourceRange; }

private:
	bool                          imageTypesMatch(VkImageType imageType) const;

	Image *const                  image = nullptr;
	const VkImageViewType         viewType = VK_IMAGE_VIEW_TYPE_2D;
	const Format                  format;
	const VkComponentMapping      components = {};
	const VkImageSubresourceRange subresourceRange = {};
};

static inline ImageView* Cast(VkImageView object)
{
	return reinterpret_cast<ImageView*>(object);
}

} // namespace vk

#endif // VK_IMAGE_VIEW_HPP_
