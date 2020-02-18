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

#ifndef VK_SEMAPHORE_EXTERNAL_FUCHSIA_H_
#define VK_SEMAPHORE_EXTERNAL_FUCHSIA_H_

#include "VkDebug.hpp"

#include <zircon/syscalls.h>

// An external semaphore implementation for the Zircon kernel using a simple
// Zircon event handle. This matches
// VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_TEMP_ZIRCON_EVENT_BIT_FUCHSIA
// which is not official yet but used by Fuchsia at the moment.

namespace vk
{

class Semaphore::External {
public:
	// The type of external semaphore handle types supported by this implementation.
	static const VkExternalSemaphoreHandleTypeFlags kExternalSemaphoreHandleType =
			VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_TEMP_ZIRCON_EVENT_BIT_FUCHSIA;

	// Default constructor. Note that one should call either init() or
	// importFd() before any call to wait() or signal().
	External() = default;

	~External()
	{
		zx_handle_close(handle);
	}

	void init()
	{
		zx_status_t status = zx_event_create(0, &handle);
		if (status != ZX_OK)
		{
			ABORT("zx_event_create() returned %d", status);
		}
	}

	void importHandle(zx_handle_t new_handle)
	{
		zx_handle_close(handle);
		handle = new_handle;
	}

	VkResult exportHandle(zx_handle_t* pHandle) const
	{
		zx_handle_t new_handle = ZX_HANDLE_INVALID;
		zx_status_t status = zx_handle_duplicate(handle, ZX_RIGHT_SAME_RIGHTS, &new_handle);
		if (status != ZX_OK)
		{
			TRACE("zx_handle_duplicate() returned %d", status);
			return VK_ERROR_INVALID_EXTERNAL_HANDLE;
		}
		*pHandle = new_handle;
		return VK_SUCCESS;
	}

	void wait()
	{
		zx_signals_t observed = 0;
		zx_status_t status = zx_object_wait_one(
				handle, ZX_EVENT_SIGNALED, ZX_TIME_INFINITE, &observed);
		if (status != ZX_OK)
		{
			ABORT("zx_object_wait_one() returned %d", status);
		}
		if (observed != ZX_EVENT_SIGNALED)
		{
			ABORT("zx_object_wait_one() returned observed %x (%x expected)", observed, ZX_EVENT_SIGNALED);
		}
		// Need to unsignal the event now, as required by the Vulkan spec.
		status = zx_object_signal(handle, ZX_EVENT_SIGNALED, 0);
		if (status != ZX_OK)
		{
			ABORT("zx_object_signal() returned %d", status);
		}
	}

	bool tryWait()
	{
		zx_signals_t observed = 0;
		zx_status_t status = zx_object_wait_one(
				handle, ZX_EVENT_SIGNALED, zx_clock_get_monotonic(), &observed);
		if (status != ZX_OK)
		{
			ABORT("zx_object_wait_one() returned %d", status);
		}
		if (observed != ZX_EVENT_SIGNALED)
		{
			return false;
		}
		// Need to unsignal the event now, as required by the Vulkan spec.
		status = zx_object_signal(handle, ZX_EVENT_SIGNALED, 0);
		if (status != ZX_OK)
		{
			ABORT("zx_object_signal() returned %d", status);
		}
		return true;
	}

	void signal()
	{
		zx_status_t status = zx_object_signal(handle, 0, ZX_EVENT_SIGNALED);
		if (status != ZX_OK)
		{
			ABORT("zx_object_signal() returned %d", status);
		}
	}

private:
	zx_handle_t handle = ZX_HANDLE_INVALID;
};

}  // namespace vk

#endif  // VK_SEMAPHORE_EXTERNAL_FUCHSIA_H_
