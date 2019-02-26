// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/vulkan_implementation_scenic.h"

#include <lib/ui/scenic/cpp/commands.h>
#include <lib/ui/scenic/cpp/session.h>
#include <lib/zx/channel.h>

#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/macros.h"
#include "base/native_library.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_instance.h"
#include "gpu/vulkan/vulkan_surface.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/ozone/platform/scenic/scenic_window.h"
#include "ui/ozone/platform/scenic/scenic_window_manager.h"
#include "ui/ozone/platform/scenic/vulkan_magma.h"

namespace ui {

namespace {

// Holds resources necessary for presenting to a View using a VkSurfaceKHR.
class ScenicSurface {
 public:
  ScenicSurface(fuchsia::ui::scenic::Scenic* scenic,
                mojom::ScenicGpuHost* gpu_host,
                gfx::AcceleratedWidget window)
      : scenic_(scenic),
        parent_(&scenic_),
        shape_(&scenic_),
        material_(&scenic_),
        gpu_host_(gpu_host),
        window_(window) {
    shape_.SetShape(scenic::Rectangle(&scenic_, 1.f, 1.f));
    shape_.SetMaterial(material_);
  }

  // Sets the texture of the surface to a new image pipe.
  void SetTextureToNewImagePipe(
      fidl::InterfaceRequest<fuchsia::images::ImagePipe> image_pipe_request) {
    uint32_t image_pipe_id = scenic_.AllocResourceId();
    scenic_.Enqueue(scenic::NewCreateImagePipeCmd(
        image_pipe_id, std::move(image_pipe_request)));
    material_.SetTexture(image_pipe_id);
    scenic_.ReleaseResource(image_pipe_id);
  }

  // Links the surface to the window in the browser process.
  //
  // Scenic does not care about order here; it's totally fine for imports to
  // cause exports, and that's what's done here.
  void LinkToParent() {
    zx::eventpair export_token;
    parent_.BindAsRequest(&export_token);
    parent_.AddChild(shape_);
    gpu_host_->ExportParent(window_,
                            mojo::WrapPlatformHandle(
                                mojo::PlatformHandle(std::move(export_token))));
  }

  // Flushes commands to scenic & executes them.
  void Commit() {
    scenic_.Present(
        /*presentation_time=*/0, [](fuchsia::images::PresentationInfo info) {});
  }

 private:
  scenic::Session scenic_;
  scenic::ImportNode parent_;
  scenic::ShapeNode shape_;
  scenic::Material material_;

  mojom::ScenicGpuHost* gpu_host_ = nullptr;
  gfx::AcceleratedWidget window_ = gfx::kNullAcceleratedWidget;

  DISALLOW_COPY_AND_ASSIGN(ScenicSurface);
};

}  // namespace

VulkanImplementationScenic::VulkanImplementationScenic(
    mojom::ScenicGpuHost* scenic_gpu_host,
    fuchsia::ui::scenic::Scenic* scenic)
    : scenic_gpu_host_(scenic_gpu_host), scenic_(scenic) {}

VulkanImplementationScenic::~VulkanImplementationScenic() = default;

bool VulkanImplementationScenic::InitializeVulkanInstance() {
  base::NativeLibraryLoadError error;
  base::NativeLibrary handle =
      base::LoadNativeLibrary(base::FilePath("libvulkan.so"), &error);
  if (!handle) {
    LOG(ERROR) << "Failed to load vulkan: " << error.ToString();
    return false;
  }

  gpu::VulkanFunctionPointers* vulkan_function_pointers =
      gpu::GetVulkanFunctionPointers();
  vulkan_function_pointers->vulkan_loader_library_ = handle;
  std::vector<const char*> required_extensions = {
      VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_MAGMA_SURFACE_EXTENSION_NAME,
  };
  std::vector<const char*> required_layers = {
      "VK_LAYER_GOOGLE_image_pipe_swapchain",
  };
  if (!vulkan_instance_.Initialize(required_extensions, required_layers)) {
    vulkan_instance_.Destroy();
    return false;
  }

  vkCreateMagmaSurfaceKHR_ = vkGetInstanceProcAddr(
      vulkan_instance_.vk_instance(), "vkCreateMagmaSurfaceKHR");
  if (!vkCreateMagmaSurfaceKHR_) {
    vulkan_instance_.Destroy();
    return false;
  }

  vkGetPhysicalDeviceMagmaPresentationSupportKHR_ =
      vkGetInstanceProcAddr(vulkan_instance_.vk_instance(),
                            "vkGetPhysicalDeviceMagmaPresentationSupportKHR");
  if (!vkGetPhysicalDeviceMagmaPresentationSupportKHR_) {
    vulkan_instance_.Destroy();
    return false;
  }

  return true;
}

VkInstance VulkanImplementationScenic::GetVulkanInstance() {
  return vulkan_instance_.vk_instance();
}

std::unique_ptr<gpu::VulkanSurface>
VulkanImplementationScenic::CreateViewSurface(gfx::AcceleratedWidget window) {
  auto scenic_surface =
      std::make_unique<ScenicSurface>(scenic_, scenic_gpu_host_, window);

  // Attach the surface to the window.
  scenic_surface->LinkToParent();

  fuchsia::images::ImagePipePtr image_pipe;
  scenic_surface->SetTextureToNewImagePipe(image_pipe.NewRequest());

  VkSurfaceKHR surface;
  VkMagmaSurfaceCreateInfoKHR surface_create_info = {};
  surface_create_info.sType = VK_STRUCTURE_TYPE_MAGMA_SURFACE_CREATE_INFO_KHR;
  surface_create_info.imagePipeHandle =
      image_pipe.Unbind().TakeChannel().release();

  VkResult result =
      reinterpret_cast<PFN_vkCreateMagmaSurfaceKHR>(vkCreateMagmaSurfaceKHR_)(
          GetVulkanInstance(), &surface_create_info, nullptr, &surface);
  if (result != VK_SUCCESS) {
    // This shouldn't fail, and we don't know whether imagePipeHandle was closed
    // if it does.
    LOG(FATAL) << "vkCreateMagmaSurfaceKHR failed: " << result;
  }

  // Execute the initialization commands. Once this is done we won't need to
  // make any further changes to ScenicSurface other than to keep it alive; the
  // texture can be replaced through the vulkan swapchain API.
  scenic_surface->Commit();

  auto destruction_callback =
      base::BindOnce(base::DoNothing::Once<std::unique_ptr<ScenicSurface>>(),
                     std::move(scenic_surface));

  return std::make_unique<gpu::VulkanSurface>(GetVulkanInstance(), surface,
                                              std::move(destruction_callback));
}

bool VulkanImplementationScenic::GetPhysicalDevicePresentationSupport(
    VkPhysicalDevice physical_device,
    const std::vector<VkQueueFamilyProperties>& queue_family_properties,
    uint32_t queue_family_index) {
  // TODO(spang): vkGetPhysicalDeviceMagmaPresentationSupportKHR returns false
  // here. Use it once it is fixed.
  NOTIMPLEMENTED();
  return true;
}

std::vector<const char*>
VulkanImplementationScenic::GetRequiredDeviceExtensions() {
  return {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
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

}  // namespace ui
