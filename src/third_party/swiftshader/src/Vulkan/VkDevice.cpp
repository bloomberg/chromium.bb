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

#include "VkDevice.hpp"

#include "VkConfig.h"
#include "VkDebug.hpp"
#include "VkDescriptorSetLayout.hpp"
#include "VkFence.hpp"
#include "VkQueue.hpp"
#include "Device/Blitter.hpp"

#include <chrono>
#include <climits>
#include <new> // Must #include this to use "placement new"

namespace
{
	std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> now()
	{
		return std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now());
	}
}

namespace vk
{

Device::Device(const VkDeviceCreateInfo* pCreateInfo, void* mem, PhysicalDevice *physicalDevice)
	: physicalDevice(physicalDevice),
	  queues(reinterpret_cast<Queue*>(mem)),
	  enabledExtensionCount(pCreateInfo->enabledExtensionCount)
{
	for(uint32_t i = 0; i < pCreateInfo->queueCreateInfoCount; i++)
	{
		const VkDeviceQueueCreateInfo& queueCreateInfo = pCreateInfo->pQueueCreateInfos[i];
		queueCount += queueCreateInfo.queueCount;
	}

	uint32_t queueID = 0;
	for(uint32_t i = 0; i < pCreateInfo->queueCreateInfoCount; i++)
	{
		const VkDeviceQueueCreateInfo& queueCreateInfo = pCreateInfo->pQueueCreateInfos[i];

		for(uint32_t j = 0; j < queueCreateInfo.queueCount; j++, queueID++)
		{
			new (&queues[queueID]) Queue();
		}
	}

	extensions = reinterpret_cast<ExtensionName*>(static_cast<uint8_t*>(mem) + (sizeof(Queue) * queueCount));
	for(uint32_t i = 0; i < enabledExtensionCount; i++)
	{
		strncpy(extensions[i], pCreateInfo->ppEnabledExtensionNames[i], VK_MAX_EXTENSION_NAME_SIZE);
	}

	if(pCreateInfo->enabledLayerCount)
	{
		// "The ppEnabledLayerNames and enabledLayerCount members of VkDeviceCreateInfo are deprecated and their values must be ignored by implementations."
		UNIMPLEMENTED("enabledLayerCount");   // TODO(b/119321052): UNIMPLEMENTED() should be used only for features that must still be implemented. Use a more informational macro here.
	}

	// FIXME (b/119409619): use an allocator here so we can control all memory allocations
	blitter = new sw::Blitter();
}

void Device::destroy(const VkAllocationCallbacks* pAllocator)
{
	for(uint32_t i = 0; i < queueCount; i++)
	{
		queues[i].~Queue();
	}

	vk::deallocate(queues, pAllocator);

	delete blitter;
}

size_t Device::ComputeRequiredAllocationSize(const VkDeviceCreateInfo* pCreateInfo)
{
	uint32_t queueCount = 0;
	for(uint32_t i = 0; i < pCreateInfo->queueCreateInfoCount; i++)
	{
		queueCount += pCreateInfo->pQueueCreateInfos[i].queueCount;
	}

	return (sizeof(Queue) * queueCount) + (pCreateInfo->enabledExtensionCount * sizeof(ExtensionName));
}

bool Device::hasExtension(const char* extensionName) const
{
	for(uint32_t i = 0; i < enabledExtensionCount; i++)
	{
		if(strncmp(extensions[i], extensionName, VK_MAX_EXTENSION_NAME_SIZE) == 0)
		{
			return true;
		}
	}
	return false;
}

VkQueue Device::getQueue(uint32_t queueFamilyIndex, uint32_t queueIndex) const
{
	ASSERT(queueFamilyIndex == 0);

	return queues[queueIndex];
}

VkResult Device::waitForFences(uint32_t fenceCount, const VkFence* pFences, VkBool32 waitAll, uint64_t timeout)
{
	using time_point = std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>;
	const time_point start = now();
	const uint64_t max_timeout = (LLONG_MAX - start.time_since_epoch().count());
	bool infiniteTimeout = (timeout > max_timeout);
	const time_point end_ns = start + std::chrono::nanoseconds(std::min(max_timeout, timeout));
	if(waitAll) // All fences must be signaled
	{
		for(uint32_t i = 0; i < fenceCount; i++)
		{
			if(timeout == 0)
			{
				if(Cast(pFences[i])->getStatus() != VK_SUCCESS) // At least one fence is not signaled
				{
					return VK_TIMEOUT;
				}
			}
			else if(infiniteTimeout)
			{
				if(Cast(pFences[i])->wait() != VK_SUCCESS) // At least one fence is not signaled
				{
					return VK_TIMEOUT;
				}
			}
			else
			{
				if(Cast(pFences[i])->wait(end_ns) != VK_SUCCESS) // At least one fence is not signaled
				{
					return VK_TIMEOUT;
				}
			}
		}

		return VK_SUCCESS;
	}
	else // At least one fence must be signaled
	{
		// Start by quickly checking the status of all fences, as only one is required
		for(uint32_t i = 0; i < fenceCount; i++)
		{
			if(Cast(pFences[i])->getStatus() == VK_SUCCESS) // At least one fence is signaled
			{
				return VK_SUCCESS;
			}
		}

		if(timeout > 0)
		{
			for(uint32_t i = 0; i < fenceCount; i++)
			{
				if(infiniteTimeout)
				{
					if(Cast(pFences[i])->wait() == VK_SUCCESS) // At least one fence is signaled
					{
						return VK_SUCCESS;
					}
				}
				else
				{
					if(Cast(pFences[i])->wait(end_ns) == VK_SUCCESS) // At least one fence is signaled
					{
						return VK_SUCCESS;
					}
				}
			}
		}

		return VK_TIMEOUT;
	}
}

VkResult Device::waitIdle()
{
	for(uint32_t i = 0; i < queueCount; i++)
	{
		queues[i].waitIdle();
	}

	return VK_SUCCESS;
}

void Device::getDescriptorSetLayoutSupport(const VkDescriptorSetLayoutCreateInfo* pCreateInfo,
                                           VkDescriptorSetLayoutSupport* pSupport) const
{
	// Mark everything as unsupported
	pSupport->supported = VK_FALSE;
}

void Device::updateDescriptorSets(uint32_t descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites,
                                  uint32_t descriptorCopyCount, const VkCopyDescriptorSet* pDescriptorCopies)
{
	for(uint32_t i = 0; i < descriptorWriteCount; i++)
	{
		DescriptorSetLayout::WriteDescriptorSet(pDescriptorWrites[i]);
	}

	for(uint32_t i = 0; i < descriptorCopyCount; i++)
	{
		DescriptorSetLayout::CopyDescriptorSet(pDescriptorCopies[i]);
	}
}

} // namespace vk
