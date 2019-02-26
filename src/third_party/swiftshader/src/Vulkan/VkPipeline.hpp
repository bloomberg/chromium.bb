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

#ifndef VK_PIPELINE_HPP_
#define VK_PIPELINE_HPP_

#include "VkObject.hpp"

namespace vk
{

class Pipeline
{
public:
	virtual void destroy(const VkAllocationCallbacks* pAllocator) = 0;
#ifndef NDEBUG
	virtual VkPipelineBindPoint bindPoint() const = 0;
#endif
};

class GraphicsPipeline : public Pipeline, public Object<GraphicsPipeline, VkPipeline>
{
public:
	GraphicsPipeline(const VkGraphicsPipelineCreateInfo* pCreateInfo, void* mem);
	~GraphicsPipeline() = delete;
	void destroy(const VkAllocationCallbacks* pAllocator) override;

#ifndef NDEBUG
	VkPipelineBindPoint bindPoint() const override
	{
		return VK_PIPELINE_BIND_POINT_GRAPHICS;
	}
#endif

	static size_t ComputeRequiredAllocationSize(const VkGraphicsPipelineCreateInfo* pCreateInfo);
};

class ComputePipeline : public Pipeline, public Object<ComputePipeline, VkPipeline>
{
public:
	ComputePipeline(const VkComputePipelineCreateInfo* pCreateInfo, void* mem);
	~ComputePipeline() = delete;
	void destroy(const VkAllocationCallbacks* pAllocator) override;

#ifndef NDEBUG
	VkPipelineBindPoint bindPoint() const override
	{
		return VK_PIPELINE_BIND_POINT_COMPUTE;
	}
#endif

	static size_t ComputeRequiredAllocationSize(const VkComputePipelineCreateInfo* pCreateInfo);
};

static inline Pipeline* Cast(VkPipeline object)
{
	return reinterpret_cast<Pipeline*>(object);
}

} // namespace vk

#endif // VK_PIPELINE_HPP_
