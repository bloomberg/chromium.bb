// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_SCENIC_VULKAN_MAGMA_H_
#define UI_OZONE_PLATFORM_SCENIC_VULKAN_MAGMA_H_

// These definitions are from
// https://fuchsia.googlesource.com/third_party/vulkan_loader_and_validation_layers/+/master/include/vulkan/vulkan.h
// TODO(spang): Remove these once the definitions go upstream.

#include <vulkan/vulkan.h>

#if !defined(VK_KHR_external_memory_fuchsia)
#define VK_KHR_external_memory_fuchsia 1
#define VK_KHR_EXTERNAL_MEMORY_FUCHSIA_SPEC_VERSION 1
#define VK_KHR_EXTERNAL_MEMORY_FUCHSIA_EXTENSION_NAME \
  "VK_KHR_external_memory_fuchsia"

typedef struct VkImportMemoryFuchsiaHandleInfoKHR {
  VkStructureType sType;
  const void* pNext;
  VkExternalMemoryHandleTypeFlagBitsKHR handleType;
  uint32_t handle;
} VkImportMemoryFuchsiaHandleInfoKHR;

typedef struct VkMemoryFuchsiaHandlePropertiesKHR {
  VkStructureType sType;
  void* pNext;
  uint32_t memoryTypeBits;
} VkMemoryFuchsiaHandlePropertiesKHR;

typedef struct VkMemoryGetFuchsiaHandleInfoKHR {
  VkStructureType sType;
  const void* pNext;
  VkDeviceMemory memory;
  VkExternalMemoryHandleTypeFlagBitsKHR handleType;
} VkMemoryGetFuchsiaHandleInfoKHR;

typedef VkResult(VKAPI_PTR* PFN_vkGetMemoryFuchsiaHandleKHR)(
    VkDevice device,
    const VkMemoryGetFuchsiaHandleInfoKHR* pGetFuchsiaHandleInfo,
    uint32_t* pFuchsiaHandle);
typedef VkResult(VKAPI_PTR* PFN_vkGetMemoryFuchsiaHandlePropertiesKHR)(
    VkDevice device,
    VkExternalMemoryHandleTypeFlagBitsKHR handleType,
    uint32_t fuchsiaHandle,
    VkMemoryFuchsiaHandlePropertiesKHR* pMemoryFuchsiaHandleProperties);

#ifndef VK_NO_PROTOTYPES
VKAPI_ATTR VkResult VKAPI_CALL vkGetMemoryFuchsiaHandleKHR(
    VkDevice device,
    const VkMemoryGetFuchsiaHandleInfoKHR* pGetFuchsiaHandleInfo,
    uint32_t* pFuchsiaHandle);

VKAPI_ATTR VkResult VKAPI_CALL vkGetMemoryFuchsiaHandlePropertiesKHR(
    VkDevice device,
    VkExternalMemoryHandleTypeFlagBitsKHR handleType,
    uint32_t fuchsiaHandle,
    VkMemoryFuchsiaHandlePropertiesKHR* pMemoryFuchsiaHandleProperties);
#endif

#endif  // !defined(VK_KHR_external_memory_fuchsia)

#if !defined(VK_KHR_external_semaphore_fuchsia)
#define VK_KHR_external_semaphore_fuchsia 1
#define VK_KHR_EXTERNAL_SEMAPHORE_FUCHSIA_SPEC_VERSION 1
#define VK_KHR_EXTERNAL_SEMAPHORE_FUCHSIA_EXTENSION_NAME \
  "VK_KHR_external_semaphore_fuchsia"

typedef struct VkImportSemaphoreFuchsiaHandleInfoKHR {
  VkStructureType sType;
  const void* pNext;
  VkSemaphore semaphore;
  VkSemaphoreImportFlagsKHR flags;
  VkExternalSemaphoreHandleTypeFlagBitsKHR handleType;
  uint32_t handle;
} VkImportSemaphoreFuchsiaHandleInfoKHR;

typedef struct VkSemaphoreGetFuchsiaHandleInfoKHR {
  VkStructureType sType;
  const void* pNext;
  VkSemaphore semaphore;
  VkExternalSemaphoreHandleTypeFlagBitsKHR handleType;
} VkSemaphoreGetFuchsiaHandleInfoKHR;

typedef VkResult(VKAPI_PTR* PFN_vkImportSemaphoreFuchsiaHandleKHR)(
    VkDevice device,
    const VkImportSemaphoreFuchsiaHandleInfoKHR*
        pImportSemaphoreFuchsiaHandleInfo);
typedef VkResult(VKAPI_PTR* PFN_vkGetSemaphoreFuchsiaHandleKHR)(
    VkDevice device,
    const VkSemaphoreGetFuchsiaHandleInfoKHR* pGetFuchsiaHandleInfo,
    uint32_t* pFuchsiaHandle);

#ifndef VK_NO_PROTOTYPES
VKAPI_ATTR VkResult VKAPI_CALL
vkImportSemaphoreFuchsiaHandleKHR(VkDevice device,
                                  const VkImportSemaphoreFuchsiaHandleInfoKHR*
                                      pImportSemaphoreFuchsiaHandleInfo);

VKAPI_ATTR VkResult VKAPI_CALL vkGetSemaphoreFuchsiaHandleKHR(
    VkDevice device,
    const VkSemaphoreGetFuchsiaHandleInfoKHR* pGetFuchsiaHandleInfo,
    uint32_t* pFuchsiaHandle);
#endif

#endif  // !defined(VK_KHR_external_semaphore_fuchsia)

#if defined(VK_USE_PLATFORM_MAGMA_KHR) && !defined(VK_KHR_magma_surface)
#define VK_KHR_magma_surface 1
#define VK_KHR_MAGMA_SURFACE_SPEC_VERSION 1
#define VK_KHR_MAGMA_SURFACE_EXTENSION_NAME "VK_KHR_magma_surface"

typedef VkFlags VkMagmaSurfaceCreateFlagsKHR;

typedef struct VkMagmaSurfaceCreateInfoKHR {
  VkStructureType sType;
  const void* pNext;
  uint32_t imagePipeHandle;
  uint32_t width;
  uint32_t height;
} VkMagmaSurfaceCreateInfoKHR;

typedef VkResult(VKAPI_PTR* PFN_vkCreateMagmaSurfaceKHR)(
    VkInstance instance,
    const VkMagmaSurfaceCreateInfoKHR* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkSurfaceKHR* pSurface);
typedef VkBool32(VKAPI_PTR* PFN_vkGetPhysicalDeviceMagmaPresentationSupportKHR)(
    VkPhysicalDevice physicalDevice,
    uint32_t queueFamilyIndex);

#ifndef VK_NO_PROTOTYPES
VKAPI_ATTR VkResult VKAPI_CALL
vkCreateMagmaSurfaceKHR(VkInstance instance,
                        const VkMagmaSurfaceCreateInfoKHR* pCreateInfo,
                        const VkAllocationCallbacks* pAllocator,
                        VkSurfaceKHR* pSurface);

VKAPI_ATTR VkBool32 VKAPI_CALL
vkGetPhysicalDeviceMagmaPresentationSupportKHR(VkPhysicalDevice physicalDevice,
                                               uint32_t queueFamilyIndex);
#endif
#endif  // defined(VK_USE_PLATFORM_MAGMA_KHR) && !defined(VK_KHR_magma_surface)

#if !defined(VK_GOOGLE_image_usage_scanout)

#define VK_GOOGLE_image_usage_scanout 1
#define VK_GOOGLE_IMAGE_USAGE_SCANOUT_SPEC_VERSION 1
#define VK_GOOGLE_IMAGE_USAGE_SCANOUT_EXTENSION_NAME \
  "VK_GOOGLE_image_usage_scanout"

#define VK_STRUCTURE_TYPE_MAGMA_SURFACE_CREATE_INFO_KHR \
  (static_cast<VkStructureType>(1001002000))

#endif  // !defined(VK_GOOGLE_image_usage_scanout)

#endif  // UI_OZONE_PLATFORM_MAGMA_VULKAN_MAGMA_H_
