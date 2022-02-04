// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/vulkan_implementation_scenic.h"

#include <lib/ui/scenic/cpp/commands.h>
#include <lib/ui/scenic/cpp/session.h>
#include <lib/zx/channel.h>
#include <vulkan/vulkan.h>

#include <memory>
#include <tuple>

#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/native_library.h"
#include "gpu/ipc/common/vulkan_ycbcr_info.h"
#include "gpu/vulkan/fuchsia/vulkan_fuchsia_ext.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_image.h"
#include "gpu/vulkan/vulkan_instance.h"
#include "gpu/vulkan/vulkan_surface.h"
#include "gpu/vulkan/vulkan_util.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/ozone/platform/scenic/scenic_surface.h"
#include "ui/ozone/platform/scenic/scenic_surface_factory.h"
#include "ui/ozone/platform/scenic/scenic_window.h"
#include "ui/ozone/platform/scenic/scenic_window_manager.h"
#include "ui/ozone/platform/scenic/sysmem_buffer_collection.h"

namespace ui {

namespace {

constexpr char kFuchsiaSwapchainLayerName[] =
    "VK_LAYER_FUCHSIA_imagepipe_swapchain";

bool CheckSwapchainAvailable() {
  uint32_t num_instance_exts;
  VkResult result = vkEnumerateInstanceExtensionProperties(
      kFuchsiaSwapchainLayerName, &num_instance_exts, nullptr);
  return result == VK_SUCCESS;
}

bool IsSwapchainEnabled() {
  static bool is_swapchain_enabled = CheckSwapchainAvailable();
  return is_swapchain_enabled;
}

}  // namespace

VulkanImplementationScenic::VulkanImplementationScenic(
    ScenicSurfaceFactory* scenic_surface_factory,
    SysmemBufferManager* sysmem_buffer_manager,
    bool allow_protected_memory)
    : VulkanImplementation(false /* use_swiftshader */, allow_protected_memory),
      scenic_surface_factory_(scenic_surface_factory),
      sysmem_buffer_manager_(sysmem_buffer_manager) {}

VulkanImplementationScenic::~VulkanImplementationScenic() = default;

bool VulkanImplementationScenic::InitializeVulkanInstance(bool using_surface) {
  DCHECK(using_surface);
  base::NativeLibraryLoadError error;
  base::NativeLibrary handle =
      base::LoadNativeLibrary(base::FilePath("libvulkan.so"), &error);
  if (!handle) {
    LOG(ERROR) << "Failed to load vulkan: " << error.ToString();
    return false;
  }

  gpu::VulkanFunctionPointers* vulkan_function_pointers =
      gpu::GetVulkanFunctionPointers();
  vulkan_function_pointers->vulkan_loader_library = handle;

  if (!vulkan_function_pointers->BindUnassociatedFunctionPointers())
    return false;

  std::vector<const char*> required_extensions = {
      VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
  };
  std::vector<const char*> required_layers;

  if (IsSwapchainEnabled()) {
    required_layers.push_back(kFuchsiaSwapchainLayerName);
    required_extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
    required_extensions.push_back(VK_FUCHSIA_IMAGEPIPE_SURFACE_EXTENSION_NAME);
  };

  return vulkan_instance_.Initialize(required_extensions, required_layers);
}

gpu::VulkanInstance* VulkanImplementationScenic::GetVulkanInstance() {
  return &vulkan_instance_;
}

std::unique_ptr<gpu::VulkanSurface>
VulkanImplementationScenic::CreateViewSurface(gfx::AcceleratedWidget window) {
  if (!IsSwapchainEnabled()) {
    LOG(FATAL) << "CreateViewSurface() called while swapchain extension isn't "
                  "enabled.";
  }

  ScenicSurface* scenic_surface = scenic_surface_factory_->GetSurface(window);
  fuchsia::images::ImagePipe2Ptr image_pipe;
  scenic_surface->SetTextureToNewImagePipe(image_pipe.NewRequest());
  zx_handle_t image_pipe_handle = image_pipe.Unbind().TakeChannel().release();

  VkSurfaceKHR surface;
  VkImagePipeSurfaceCreateInfoFUCHSIA surface_create_info = {};
  surface_create_info.sType =
      VK_STRUCTURE_TYPE_IMAGEPIPE_SURFACE_CREATE_INFO_FUCHSIA;
  surface_create_info.flags = 0;
  surface_create_info.imagePipeHandle = image_pipe_handle;

  VkResult result = vkCreateImagePipeSurfaceFUCHSIA(
      vulkan_instance_.vk_instance(), &surface_create_info, nullptr, &surface);
  if (result != VK_SUCCESS) {
    // This shouldn't fail, and we don't know whether imagePipeHandle was closed
    // if it does.
    LOG(FATAL) << "vkCreateImagePipeSurfaceFUCHSIA failed: " << result;
  }

  return std::make_unique<gpu::VulkanSurface>(vulkan_instance_.vk_instance(),
                                              window, surface);
}

bool VulkanImplementationScenic::GetPhysicalDevicePresentationSupport(
    VkPhysicalDevice physical_device,
    const std::vector<VkQueueFamilyProperties>& queue_family_properties,
    uint32_t queue_family_index) {
  return true;
}

std::vector<const char*>
VulkanImplementationScenic::GetRequiredDeviceExtensions() {
  std::vector<const char*> result = {
      VK_FUCHSIA_BUFFER_COLLECTION_EXTENSION_NAME,
      VK_FUCHSIA_EXTERNAL_MEMORY_EXTENSION_NAME,
      VK_FUCHSIA_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
      VK_KHR_BIND_MEMORY_2_EXTENSION_NAME,
      VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
      VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
      VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
      VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
      VK_KHR_MAINTENANCE1_EXTENSION_NAME,
      VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME,
  };

  if (IsSwapchainEnabled())
    result.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

  return result;
}

std::vector<const char*>
VulkanImplementationScenic::GetOptionalDeviceExtensions() {
  return {};
}

VkFence VulkanImplementationScenic::CreateVkFenceForGpuFence(
    VkDevice vk_device) {
  NOTIMPLEMENTED();
  return VK_NULL_HANDLE;
}

std::unique_ptr<gfx::GpuFence>
VulkanImplementationScenic::ExportVkFenceToGpuFence(VkDevice vk_device,
                                                    VkFence vk_fence) {
  NOTIMPLEMENTED();
  return nullptr;
}

VkSemaphore VulkanImplementationScenic::CreateExternalSemaphore(
    VkDevice vk_device) {
  return gpu::CreateExternalVkSemaphore(
      vk_device, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_ZIRCON_EVENT_BIT_FUCHSIA);
}

VkSemaphore VulkanImplementationScenic::ImportSemaphoreHandle(
    VkDevice vk_device,
    gpu::SemaphoreHandle handle) {
  if (!handle.is_valid())
    return VK_NULL_HANDLE;

  if (handle.vk_handle_type() !=
      VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_ZIRCON_EVENT_BIT_FUCHSIA) {
    return VK_NULL_HANDLE;
  }

  VkSemaphore semaphore = VK_NULL_HANDLE;
  VkSemaphoreCreateInfo info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  VkResult result = vkCreateSemaphore(vk_device, &info, nullptr, &semaphore);
  if (result != VK_SUCCESS)
    return VK_NULL_HANDLE;

  zx::event event = handle.TakeHandle();
  VkImportSemaphoreZirconHandleInfoFUCHSIA import = {
      VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_ZIRCON_HANDLE_INFO_FUCHSIA};
  import.semaphore = semaphore;
  import.handleType =
      VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_ZIRCON_EVENT_BIT_FUCHSIA;
  import.zirconHandle = event.get();

  result = vkImportSemaphoreZirconHandleFUCHSIA(vk_device, &import);
  if (result != VK_SUCCESS) {
    vkDestroySemaphore(vk_device, semaphore, nullptr);
    return VK_NULL_HANDLE;
  }

  // Vulkan took ownership of the handle.
  std::ignore = event.release();

  return semaphore;
}

gpu::SemaphoreHandle VulkanImplementationScenic::GetSemaphoreHandle(
    VkDevice vk_device,
    VkSemaphore vk_semaphore) {
  // Create VkSemaphoreGetFdInfoKHR structure.
  VkSemaphoreGetZirconHandleInfoFUCHSIA info = {
      VK_STRUCTURE_TYPE_SEMAPHORE_GET_ZIRCON_HANDLE_INFO_FUCHSIA};
  info.semaphore = vk_semaphore;
  info.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_ZIRCON_EVENT_BIT_FUCHSIA;

  zx_handle_t handle;
  VkResult result =
      vkGetSemaphoreZirconHandleFUCHSIA(vk_device, &info, &handle);
  if (result != VK_SUCCESS) {
    LOG(ERROR) << "vkGetSemaphoreFuchsiaHandleKHR failed : " << result;
    return gpu::SemaphoreHandle();
  }

  return gpu::SemaphoreHandle(
      VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_ZIRCON_EVENT_BIT_FUCHSIA,
      zx::event(handle));
}

VkExternalMemoryHandleTypeFlagBits
VulkanImplementationScenic::GetExternalImageHandleType() {
  return VK_EXTERNAL_MEMORY_HANDLE_TYPE_ZIRCON_VMO_BIT_FUCHSIA;
}

bool VulkanImplementationScenic::CanImportGpuMemoryBuffer(
    gfx::GpuMemoryBufferType memory_buffer_type) {
  return memory_buffer_type == gfx::NATIVE_PIXMAP;
}

std::unique_ptr<gpu::VulkanImage>
VulkanImplementationScenic::CreateImageFromGpuMemoryHandle(
    gpu::VulkanDeviceQueue* device_queue,
    gfx::GpuMemoryBufferHandle gmb_handle,
    gfx::Size size,
    VkFormat vk_format) {
  if (gmb_handle.type != gfx::NATIVE_PIXMAP)
    return nullptr;

  if (!gmb_handle.native_pixmap_handle.buffer_collection_id) {
    DLOG(ERROR) << "NativePixmapHandle.buffer_collection_id is not set.";
    return nullptr;
  }

  auto collection = sysmem_buffer_manager_->GetCollectionById(
      gmb_handle.native_pixmap_handle.buffer_collection_id.value());
  if (!collection) {
    DLOG(ERROR) << "Tried to use an unknown buffer collection ID.";
    return nullptr;
  }
  VkImage vk_image = VK_NULL_HANDLE;
  VkImageCreateInfo vk_image_info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  VkDeviceMemory vk_device_memory = VK_NULL_HANDLE;
  VkDeviceSize vk_device_size = 0;
  absl::optional<gpu::VulkanYCbCrInfo> ycbcr_info;
  if (!collection->CreateVkImage(gmb_handle.native_pixmap_handle.buffer_index,
                                 device_queue->GetVulkanDevice(), size,
                                 &vk_image, &vk_image_info, &vk_device_memory,
                                 &vk_device_size, &ycbcr_info)) {
    DLOG(ERROR) << "CreateVkImage failed.";
    return nullptr;
  }

  auto image = gpu::VulkanImage::Create(
      device_queue, vk_image, vk_device_memory, size, vk_image_info.format,
      vk_image_info.tiling, vk_device_size, 0 /* memory_type_index */,
      ycbcr_info, vk_image_info.usage, vk_image_info.flags);

  if (image->format() != vk_format) {
    DLOG(ERROR) << "Unexpected format " << vk_format << " vs "
                << image->format();
    image->Destroy();
    return nullptr;
  }

  image->set_native_pixmap(collection->CreateNativePixmap(
      gmb_handle.native_pixmap_handle.buffer_index));
  return image;
}

class SysmemBufferCollectionImpl : public gpu::SysmemBufferCollection {
 public:
  SysmemBufferCollectionImpl(
      scoped_refptr<ui::SysmemBufferCollection> collection)
      : collection_(std::move(collection)) {}

  SysmemBufferCollectionImpl(const SysmemBufferCollectionImpl&) = delete;
  SysmemBufferCollectionImpl& operator=(const SysmemBufferCollectionImpl&) =
      delete;

  ~SysmemBufferCollectionImpl() override = default;

 private:
  scoped_refptr<ui::SysmemBufferCollection> collection_;
};

std::unique_ptr<gpu::SysmemBufferCollection>
VulkanImplementationScenic::RegisterSysmemBufferCollection(
    VkDevice device,
    gfx::SysmemBufferCollectionId id,
    zx::channel token,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gfx::Size size,
    size_t min_buffer_count,
    bool register_with_image_pipe) {
  fuchsia::images::ImagePipe2Ptr image_pipe = nullptr;
  auto buffer_collection = sysmem_buffer_manager_->ImportSysmemBufferCollection(
      device, id, std::move(token), size, format, usage, min_buffer_count,
      register_with_image_pipe);
  if (!buffer_collection)
    return nullptr;

  return std::make_unique<SysmemBufferCollectionImpl>(
      std::move(buffer_collection));
}

}  // namespace ui
