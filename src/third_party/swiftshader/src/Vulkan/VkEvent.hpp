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

#ifndef VK_EVENT_HPP_
#define VK_EVENT_HPP_

#include "VkObject.hpp"
#include <condition_variable>
#include <mutex>

namespace vk
{

class Event : public Object<Event, VkEvent>
{
public:
	Event(const VkEventCreateInfo* pCreateInfo, void* mem)
	{
	}

	static size_t ComputeRequiredAllocationSize(const VkEventCreateInfo* pCreateInfo)
	{
		return 0;
	}

	void signal()
	{
		std::unique_lock<std::mutex> lock(mutex);
		status = VK_EVENT_SET;
		lock.unlock();
		condition.notify_all();
	}

	void reset()
	{
		std::unique_lock<std::mutex> lock(mutex);
		status = VK_EVENT_RESET;
	}

	VkResult getStatus()
	{
		std::unique_lock<std::mutex> lock(mutex);
		auto result = status;
		lock.unlock();
		return result;
	}

	void wait()
	{
		std::unique_lock<std::mutex> lock(mutex);
		condition.wait(lock, [this] { return status == VK_EVENT_SET; });
	}

private:
	VkResult status = VK_EVENT_RESET; // guarded by mutex
	std::mutex mutex;
	std::condition_variable condition;
};

static inline Event* Cast(VkEvent object)
{
	return Event::Cast(object);
}

} // namespace vk

#endif // VK_EVENT_HPP_
