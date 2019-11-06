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

#ifndef SWIFTSHADER_VKSWAPCHAINKHR_HPP
#define SWIFTSHADER_VKSWAPCHAINKHR_HPP


#include "Vulkan/VkObject.hpp"
#include "Vulkan/VkImage.hpp"
#include "VkSurfaceKHR.hpp"

#include <vector>

namespace vk
{

class Fence;
class Semaphore;

class SwapchainKHR : public Object<SwapchainKHR, VkSwapchainKHR>
{
public:
	SwapchainKHR(const VkSwapchainCreateInfoKHR* pCreateInfo, void* mem);

	void destroy(const VkAllocationCallbacks* pAllocator);

	static size_t ComputeRequiredAllocationSize(const VkSwapchainCreateInfoKHR* pCreateInfo);

	void retire();

	VkResult createImages(VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo);

	uint32_t getImageCount() const;
	VkResult getImages(uint32_t *pSwapchainImageCount, VkImage *pSwapchainImages) const;

	VkResult getNextImage(uint64_t timeout, Semaphore* semaphore, Fence* fence, uint32_t* pImageIndex);

	void present(uint32_t index);
	PresentImage const &getImage(uint32_t imageIndex) { return images[imageIndex]; }

private:
	SurfaceKHR* surface = nullptr;
	PresentImage* images = nullptr;
	uint32_t imageCount = 0;
	bool retired = false;

	void resetImages();
};

static inline SwapchainKHR* Cast(VkSwapchainKHR object)
{
	return SwapchainKHR::Cast(object);
}

}

#endif //SWIFTSHADER_VKSWAPCHAINKHR_HPP
