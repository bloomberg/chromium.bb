// Copyright 2020 The Dawn Authors
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

#ifndef SRC_DAWN_NATIVE_VULKAN_DESCRIPTORSETALLOCATION_H_
#define SRC_DAWN_NATIVE_VULKAN_DESCRIPTORSETALLOCATION_H_

#include "dawn/common/vulkan_platform.h"

namespace dawn::native::vulkan {

// Contains a descriptor set along with data necessary to track its allocation.
struct DescriptorSetAllocation {
    VkDescriptorSet set = VK_NULL_HANDLE;
    uint32_t poolIndex;
    uint16_t setIndex;
};

}  // namespace dawn::native::vulkan

#endif  // SRC_DAWN_NATIVE_VULKAN_DESCRIPTORSETALLOCATION_H_
