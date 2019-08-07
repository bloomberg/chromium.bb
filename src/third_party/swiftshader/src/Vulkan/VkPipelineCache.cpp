// Copyright 2019 The SwiftShader Authors. All Rights Reserved.
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

#include "VkPipelineCache.hpp"
#include <cstring>

namespace vk
{

PipelineCache::PipelineCache(const VkPipelineCacheCreateInfo* pCreateInfo, void* mem) :
	dataSize(ComputeRequiredAllocationSize(pCreateInfo)), data(reinterpret_cast<uint8_t*>(mem))
{
	CacheHeader* header = reinterpret_cast<CacheHeader*>(mem);
	header->headerLength = sizeof(CacheHeader);
	header->headerVersion = VK_PIPELINE_CACHE_HEADER_VERSION_ONE;
	header->vendorID = VENDOR_ID;
	header->deviceID = DEVICE_ID;
	memcpy(header->pipelineCacheUUID, SWIFTSHADER_UUID, VK_UUID_SIZE);

	if(pCreateInfo->pInitialData && (pCreateInfo->initialDataSize > 0))
	{
		memcpy(data + sizeof(CacheHeader), pCreateInfo->pInitialData, pCreateInfo->initialDataSize);
	}
}

void PipelineCache::destroy(const VkAllocationCallbacks* pAllocator)
{
	vk::deallocate(data, pAllocator);
}

size_t PipelineCache::ComputeRequiredAllocationSize(const VkPipelineCacheCreateInfo* pCreateInfo)
{
	return pCreateInfo->initialDataSize + sizeof(CacheHeader);
}

VkResult PipelineCache::getData(size_t* pDataSize, void* pData)
{
	if(!pData)
	{
		*pDataSize = dataSize;
		return VK_SUCCESS;
	}

	if(*pDataSize != dataSize)
	{
		*pDataSize = 0;
		return VK_INCOMPLETE;
	}

	if(*pDataSize > 0)
	{
		memcpy(pData, data, *pDataSize);
	}

	return VK_SUCCESS;
}

VkResult PipelineCache::merge(uint32_t srcCacheCount, const VkPipelineCache* pSrcCaches)
{
	for(uint32_t i = 0; i < srcCacheCount; i++)
	{
		// TODO (b/123588002): merge pSrcCaches[i];
	}

	return VK_SUCCESS;
}

} // namespace vk
