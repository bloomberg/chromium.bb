// Copyright 2017 The Dawn Authors
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

#ifndef SRC_DAWN_NATIVE_VULKAN_VULKANINFO_H_
#define SRC_DAWN_NATIVE_VULKAN_VULKANINFO_H_

#include "dawn/common/ityp_array.h"
#include "dawn/common/vulkan_platform.h"
#include "dawn/native/Error.h"
#include "dawn/native/vulkan/VulkanExtensions.h"

#include <vector>

namespace dawn::native::vulkan {

    class Adapter;
    class Backend;
    struct VulkanFunctions;

    // Global information - gathered before the instance is created
    struct VulkanGlobalKnobs {
        VulkanLayerSet layers;
        ityp::array<VulkanLayer, InstanceExtSet, static_cast<uint32_t>(VulkanLayer::EnumCount)>
            layerExtensions;

        // During information gathering `extensions` only contains the instance's extensions but
        // during the instance creation logic it becomes the OR of the instance's extensions and
        // the selected layers' extensions.
        InstanceExtSet extensions;
        bool HasExt(InstanceExt ext) const;
    };

    struct VulkanGlobalInfo : VulkanGlobalKnobs {
        uint32_t apiVersion;
    };

    // Device information - gathered before the device is created.
    struct VulkanDeviceKnobs {
        VkPhysicalDeviceFeatures features;
        VkPhysicalDeviceShaderFloat16Int8FeaturesKHR shaderFloat16Int8Features;
        VkPhysicalDevice16BitStorageFeaturesKHR _16BitStorageFeatures;
        VkPhysicalDeviceSubgroupSizeControlFeaturesEXT subgroupSizeControlFeatures;
        VkPhysicalDeviceZeroInitializeWorkgroupMemoryFeaturesKHR
            zeroInitializeWorkgroupMemoryFeatures;

        bool HasExt(DeviceExt ext) const;
        DeviceExtSet extensions;
    };

    struct VulkanDeviceInfo : VulkanDeviceKnobs {
        VkPhysicalDeviceProperties properties;
        VkPhysicalDeviceDriverProperties driverProperties;
        VkPhysicalDeviceSubgroupSizeControlPropertiesEXT subgroupSizeControlProperties;

        std::vector<VkQueueFamilyProperties> queueFamilies;

        std::vector<VkMemoryType> memoryTypes;
        std::vector<VkMemoryHeap> memoryHeaps;

        std::vector<VkLayerProperties> layers;
        // TODO(cwallez@chromium.org): layer instance extensions
    };

    struct VulkanSurfaceInfo {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
        std::vector<bool> supportedQueueFamilies;
    };

    ResultOrError<VulkanGlobalInfo> GatherGlobalInfo(const VulkanFunctions& vkFunctions);
    ResultOrError<std::vector<VkPhysicalDevice>> GatherPhysicalDevices(
        VkInstance instance,
        const VulkanFunctions& vkFunctions);
    ResultOrError<VulkanDeviceInfo> GatherDeviceInfo(const Adapter& adapter);
    ResultOrError<VulkanSurfaceInfo> GatherSurfaceInfo(const Adapter& adapter,
                                                       VkSurfaceKHR surface);
}  // namespace dawn::native::vulkan

#endif  // SRC_DAWN_NATIVE_VULKAN_VULKANINFO_H_
