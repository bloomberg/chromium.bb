// Copyright 2019 The Dawn Authors
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

#ifndef SRC_DAWN_NATIVE_VULKAN_ADAPTERVK_H_
#define SRC_DAWN_NATIVE_VULKAN_ADAPTERVK_H_

#include "dawn/native/Adapter.h"

#include "dawn/common/RefCounted.h"
#include "dawn/common/vulkan_platform.h"
#include "dawn/native/vulkan/VulkanInfo.h"

namespace dawn::native::vulkan {

    class VulkanInstance;

    class Adapter : public AdapterBase {
      public:
        Adapter(InstanceBase* instance,
                VulkanInstance* vulkanInstance,
                VkPhysicalDevice physicalDevice);
        ~Adapter() override = default;

        // AdapterBase Implementation
        bool SupportsExternalImages() const override;

        const VulkanDeviceInfo& GetDeviceInfo() const;
        VkPhysicalDevice GetPhysicalDevice() const;
        VulkanInstance* GetVulkanInstance() const;

        bool IsDepthStencilFormatSupported(VkFormat format);

      private:
        MaybeError InitializeImpl() override;
        MaybeError InitializeSupportedFeaturesImpl() override;
        MaybeError InitializeSupportedLimitsImpl(CombinedLimits* limits) override;

        ResultOrError<Ref<DeviceBase>> CreateDeviceImpl(
            const DeviceDescriptor* descriptor) override;

        VkPhysicalDevice mPhysicalDevice;
        Ref<VulkanInstance> mVulkanInstance;
        VulkanDeviceInfo mDeviceInfo = {};
    };

}  // namespace dawn::native::vulkan

#endif  // SRC_DAWN_NATIVE_VULKAN_ADAPTERVK_H_
