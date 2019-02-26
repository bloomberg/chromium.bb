// Copyright 2018 The Dawn Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef DAWNNATIVE_VULKANBACKEND_H_
#define DAWNNATIVE_VULKANBACKEND_H_

#include <dawn/dawn.h>
#include <dawn/dawn_wsi.h>
#include <dawn_native/dawn_native_export.h>

#include <vulkan/vulkan.h>

#include <vector>

namespace dawn_native { namespace vulkan {
    DAWN_NATIVE_EXPORT dawnDevice
    CreateDevice(const std::vector<const char*>& requiredInstanceExtensions);

    DAWN_NATIVE_EXPORT VkInstance GetInstance(dawnDevice device);

    DAWN_NATIVE_EXPORT dawnSwapChainImplementation CreateNativeSwapChainImpl(dawnDevice device,
                                                                             VkSurfaceKHR surface);
    DAWN_NATIVE_EXPORT dawnTextureFormat
    GetNativeSwapChainPreferredFormat(const dawnSwapChainImplementation* swapChain);
}}  // namespace dawn_native::vulkan

#endif  // DAWNNATIVE_VULKANBACKEND_H_
