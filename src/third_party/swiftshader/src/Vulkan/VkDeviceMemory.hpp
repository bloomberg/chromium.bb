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

#ifndef VK_DEVICE_MEMORY_HPP_
#define VK_DEVICE_MEMORY_HPP_

#include "VkObject.hpp"

namespace vk
{

class DeviceMemory : public Object<DeviceMemory, VkDeviceMemory>
{
public:
	DeviceMemory(const VkMemoryAllocateInfo* pCreateInfo, void* mem);

	static size_t ComputeRequiredAllocationSize(const VkMemoryAllocateInfo* pCreateInfo);

	void destroy(const VkAllocationCallbacks* pAllocator);
	VkResult allocate();
	VkResult map(VkDeviceSize offset, VkDeviceSize size, void** ppData);
	VkDeviceSize getCommittedMemoryInBytes() const;
	void* getOffsetPointer(VkDeviceSize pOffset) const;
	uint32_t getMemoryTypeIndex() const { return memoryTypeIndex; }

private:
	void*        buffer = nullptr;
	VkDeviceSize size = 0;
	uint32_t     memoryTypeIndex = 0;
};

static inline DeviceMemory* Cast(VkDeviceMemory object)
{
	return DeviceMemory::Cast(object);
}


} // namespace vk

#endif // VK_DEVICE_MEMORY_HPP_
