// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/gpu/vulkan_in_process_context_provider.h"
#include "gpu/vulkan/buildflags.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "gpu/vulkan/vulkan_instance.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace viz {

scoped_refptr<VulkanInProcessContextProvider>
VulkanInProcessContextProvider::Create(
    gpu::VulkanImplementation* vulkan_implementation) {
  scoped_refptr<VulkanInProcessContextProvider> context_provider(
      new VulkanInProcessContextProvider(vulkan_implementation));
  if (!context_provider->Initialize())
    return nullptr;
  return context_provider;
}

GrVkGetProc make_unified_getter(const PFN_vkGetInstanceProcAddr& iproc,
                                const PFN_vkGetDeviceProcAddr& dproc) {
  return [&iproc, &dproc](const char* proc_name, VkInstance instance,
                          VkDevice device) {
    if (device != VK_NULL_HANDLE) {
      return dproc(device, proc_name);
    }
    return iproc(instance, proc_name);
  };
}

bool VulkanInProcessContextProvider::Initialize() {
  DCHECK(!device_queue_);
  const gfx::ExtensionSet& extensions =
      vulkan_implementation_->GetVulkanInstance()->enabled_extensions();
  bool support_surface =
      gfx::HasExtension(extensions, VK_KHR_SURFACE_EXTENSION_NAME);
  uint32_t flags = gpu::VulkanDeviceQueue::GRAPHICS_QUEUE_FLAG;
  if (support_surface)
    flags |= gpu::VulkanDeviceQueue::PRESENTATION_SUPPORT_QUEUE_FLAG;
  std::unique_ptr<gpu::VulkanDeviceQueue> device_queue =
      gpu::CreateVulkanDeviceQueue(vulkan_implementation_, flags);
  if (!device_queue)
    return false;

  device_queue_ = std::move(device_queue);
  GrVkBackendContext backend_context;
  backend_context.fInstance = device_queue_->GetVulkanInstance();
  backend_context.fPhysicalDevice = device_queue_->GetVulkanPhysicalDevice();
  backend_context.fDevice = device_queue_->GetVulkanDevice();
  backend_context.fQueue = device_queue_->GetVulkanQueue();
  backend_context.fGraphicsQueueIndex = device_queue_->GetVulkanQueueIndex();
  backend_context.fInstanceVersion =
      vulkan_implementation_->GetVulkanInstance()->api_version();

  backend_context.fExtensions = 0;

  // Instance extensions.
  if (gfx::HasExtension(extensions, VK_EXT_DEBUG_REPORT_EXTENSION_NAME))
    backend_context.fExtensions |= kEXT_debug_report_GrVkExtensionFlag;
  if (gfx::HasExtension(extensions, VK_KHR_SURFACE_EXTENSION_NAME))
    backend_context.fExtensions |= kKHR_surface_GrVkExtensionFlag;

  // Device extensions.
  if (gfx::HasExtension(device_queue_->enabled_extensions(),
                        VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
    backend_context.fExtensions |= kKHR_swapchain_GrVkExtensionFlag;
  }

  backend_context.fFeatures = kGeometryShader_GrVkFeatureFlag |
                              kDualSrcBlend_GrVkFeatureFlag |
                              kSampleRateShading_GrVkFeatureFlag;

  gpu::VulkanFunctionPointers* vulkan_function_pointers =
      gpu::GetVulkanFunctionPointers();
  backend_context.fGetProc =
      make_unified_getter(vulkan_function_pointers->vkGetInstanceProcAddrFn,
                          vulkan_function_pointers->vkGetDeviceProcAddrFn);
  backend_context.fOwnsInstanceAndDevice = false;
  gr_context_ = GrContext::MakeVulkan(backend_context);

  return gr_context_ != nullptr;
}

void VulkanInProcessContextProvider::Destroy() {
  if (gr_context_)
    gr_context_.reset();

  if (device_queue_) {
    device_queue_->Destroy();
    device_queue_.reset();
  }
}

gpu::VulkanImplementation*
VulkanInProcessContextProvider::GetVulkanImplementation() {
  return vulkan_implementation_;
}

gpu::VulkanDeviceQueue* VulkanInProcessContextProvider::GetDeviceQueue() {
  return device_queue_.get();
}

GrContext* VulkanInProcessContextProvider::GetGrContext() {
  return gr_context_.get();
}

GrVkSecondaryCBDrawContext*
VulkanInProcessContextProvider::GetGrSecondaryCBDrawContext() {
  return nullptr;
}

void VulkanInProcessContextProvider::EnqueueSecondaryCBSemaphores(
    std::vector<VkSemaphore> semaphores) {
  NOTREACHED();
}

void VulkanInProcessContextProvider::EnqueueSecondaryCBPostSubmitTask(
    base::OnceClosure closure) {
  NOTREACHED();
}

VulkanInProcessContextProvider::VulkanInProcessContextProvider(
    gpu::VulkanImplementation* vulkan_implementation)
    : vulkan_implementation_(vulkan_implementation) {}

VulkanInProcessContextProvider::~VulkanInProcessContextProvider() {
  Destroy();
}

}  // namespace viz
