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

#ifndef VK_QUEUE_HPP_
#define VK_QUEUE_HPP_

#include "VkObject.hpp"
#include "Device/Renderer.hpp"
#include <thread>
#include <vulkan/vk_icd.h>

#include "System/Synchronization.hpp"

namespace sw
{
	class Context;
	class Renderer;
}

namespace vk
{

class Queue
{
	VK_LOADER_DATA loaderData = { ICD_LOADER_MAGIC };

public:
	Queue();
	~Queue();

	operator VkQueue()
	{
		return reinterpret_cast<VkQueue>(this);
	}

	VkResult submit(uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence);
	VkResult waitIdle();
#ifndef __ANDROID__
	void present(const VkPresentInfoKHR* presentInfo);
#endif

private:
	struct Task
	{
		uint32_t submitCount = 0;
		VkSubmitInfo* pSubmits = nullptr;
		sw::TaskEvents* events = nullptr;

		enum Type { KILL_THREAD, SUBMIT_QUEUE };
		Type type = SUBMIT_QUEUE;
	};

	static void TaskLoop(vk::Queue* queue);
	void taskLoop();
	void garbageCollect();
	void submitQueue(const Task& task);

	sw::Renderer renderer;
	sw::Chan<Task> pending;
	sw::Chan<VkSubmitInfo*> toDelete;
	std::thread queueThread;
};

static inline Queue* Cast(VkQueue object)
{
	return reinterpret_cast<Queue*>(object);
}

} // namespace vk

#endif // VK_QUEUE_HPP_
