// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shared_image_representation_skia_vk_ozone.h"

#include <utility>

#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/external_semaphore_pool.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_image.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "gpu/vulkan/vulkan_util.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"

namespace gpu {

// Vk backed Skia representation of SharedImageBackingOzone.
SharedImageRepresentationSkiaVkOzone::SharedImageRepresentationSkiaVkOzone(
    SharedImageManager* manager,
    SharedImageBackingOzone* backing,
    scoped_refptr<SharedContextState> context_state,
    std::unique_ptr<VulkanImage> vulkan_image,
    MemoryTypeTracker* tracker)
    : SharedImageRepresentationSkia(manager, backing, tracker),
      vulkan_image_(std::move(vulkan_image)),
      context_state_(std::move(context_state)) {
  DCHECK(backing);
  DCHECK(context_state_);
  DCHECK(context_state_->vk_context_provider());
  DCHECK(vulkan_image_);

  promise_texture_ = SkPromiseImageTexture::Make(
      GrBackendTexture(size().width(), size().height(),
                       CreateGrVkImageInfo(vulkan_image_.get())));
  DCHECK(promise_texture_);
}

SharedImageRepresentationSkiaVkOzone::~SharedImageRepresentationSkiaVkOzone() {
  DCHECK_EQ(mode_, RepresentationAccessMode::kNone);
  surface_.reset();
  if (vulkan_image_) {
    VulkanFenceHelper* fence_helper = context_state_->vk_context_provider()
                                          ->GetDeviceQueue()
                                          ->GetFenceHelper();
    fence_helper->EnqueueVulkanObjectCleanupForSubmittedWork(
        std::move(vulkan_image_));
  }
}

sk_sp<SkSurface> SharedImageRepresentationSkiaVkOzone::BeginWriteAccess(
    int final_msaa_count,
    const SkSurfaceProps& surface_props,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    std::unique_ptr<GrBackendSurfaceMutableState>* end_state) {
  DCHECK_EQ(mode_, RepresentationAccessMode::kNone);
  DCHECK(promise_texture_);

  if (!BeginAccess(/*readonly=*/false, begin_semaphores, end_semaphores))
    return nullptr;

  auto* gr_context = context_state_->gr_context();
  if (gr_context->abandoned()) {
    LOG(ERROR) << "GrContext is abandoned.";
    return nullptr;
  }

  if (!surface_ || final_msaa_count != surface_msaa_count_ ||
      surface_props != surface_->props()) {
    SkColorType sk_color_type = viz::ResourceFormatToClosestSkColorType(
        /*gpu_compositing=*/true, format());
    surface_ = SkSurface::MakeFromBackendTexture(
        gr_context, promise_texture_->backendTexture(), surface_origin(),
        final_msaa_count, sk_color_type, color_space().ToSkColorSpace(),
        &surface_props);
    if (!surface_) {
      LOG(ERROR) << "MakeFromBackendTexture() failed.";
      return nullptr;
    }
    surface_msaa_count_ = final_msaa_count;
  }

  *end_state = GetEndAccessState();
  return surface_;
}

sk_sp<SkPromiseImageTexture>
SharedImageRepresentationSkiaVkOzone::BeginWriteAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    std::unique_ptr<GrBackendSurfaceMutableState>* end_state) {
  DCHECK_EQ(mode_, RepresentationAccessMode::kNone);
  DCHECK(promise_texture_);

  if (!BeginAccess(false /* readonly */, begin_semaphores, end_semaphores))
    return nullptr;

  *end_state = GetEndAccessState();
  return promise_texture_;
}

void SharedImageRepresentationSkiaVkOzone::EndWriteAccess(
    sk_sp<SkSurface> surface) {
  DCHECK_EQ(mode_, RepresentationAccessMode::kWrite);
  if (surface) {
    DCHECK_EQ(surface.get(), surface_.get());

    surface.reset();
    DCHECK(surface_->unique());
  }
  EndAccess(false /* readonly */);
}

sk_sp<SkPromiseImageTexture>
SharedImageRepresentationSkiaVkOzone::BeginReadAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    std::unique_ptr<GrBackendSurfaceMutableState>* end_state) {
  DCHECK_EQ(mode_, RepresentationAccessMode::kNone);
  DCHECK(!surface_);
  DCHECK(promise_texture_);

  if (!BeginAccess(true /* readonly */, begin_semaphores, end_semaphores))
    return nullptr;

  *end_state = GetEndAccessState();
  return promise_texture_;
}

void SharedImageRepresentationSkiaVkOzone::EndReadAccess() {
  DCHECK_EQ(mode_, RepresentationAccessMode::kRead);
  DCHECK(!surface_);

  EndAccess(true /* readonly */);
}

gpu::VulkanImplementation*
SharedImageRepresentationSkiaVkOzone::vk_implementation() {
  return context_state_->vk_context_provider()->GetVulkanImplementation();
}

VkDevice SharedImageRepresentationSkiaVkOzone::vk_device() {
  return context_state_->vk_context_provider()
      ->GetDeviceQueue()
      ->GetVulkanDevice();
}

bool SharedImageRepresentationSkiaVkOzone::BeginAccess(
    bool readonly,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores) {
  DCHECK(begin_semaphores);
  DCHECK(end_access_semaphore_ == VK_NULL_HANDLE);

  if (end_semaphores) {
    end_access_semaphore_ =
        vk_implementation()->CreateExternalSemaphore(vk_device());

    if (end_access_semaphore_ == VK_NULL_HANDLE) {
      DLOG(ERROR) << "Failed to create the external semaphore.";
      return false;
    }

    end_semaphores->emplace_back();
    end_semaphores->back().initVulkan(end_access_semaphore_);
  }

  std::vector<gfx::GpuFenceHandle> fences;
  ozone_backing()->BeginAccess(&fences);

  for (auto& fence : fences) {
    VkSemaphore vk_semaphore = ExternalSemaphore::CreateFromHandle(
                                   context_state_->vk_context_provider(),
                                   SemaphoreHandle(std::move(fence)))
                                   .GetVkSemaphore();
    begin_access_semaphores_.emplace_back(vk_semaphore);
    begin_semaphores->emplace_back();
    begin_semaphores->back().initVulkan(vk_semaphore);
  }

  mode_ = readonly ? RepresentationAccessMode::kRead
                   : RepresentationAccessMode::kWrite;
  return true;
}

void SharedImageRepresentationSkiaVkOzone::EndAccess(bool readonly) {
  gfx::GpuFenceHandle fence;
  if (end_access_semaphore_ != VK_NULL_HANDLE) {
    SemaphoreHandle semaphore_handle = vk_implementation()->GetSemaphoreHandle(
        vk_device(), end_access_semaphore_);
    fence = std::move(semaphore_handle).ToGpuFenceHandle();
    DCHECK(!fence.is_null());
  }

  ozone_backing()->EndAccess(readonly, std::move(fence));

  std::vector<VkSemaphore> semaphores = begin_access_semaphores_;
  begin_access_semaphores_.clear();
  if (end_access_semaphore_ != VK_NULL_HANDLE) {
    semaphores.emplace_back(end_access_semaphore_);
    end_access_semaphore_ = VK_NULL_HANDLE;
  }
  if (!semaphores.empty()) {
    VulkanFenceHelper* fence_helper = context_state_->vk_context_provider()
                                          ->GetDeviceQueue()
                                          ->GetFenceHelper();
    fence_helper->EnqueueSemaphoresCleanupForSubmittedWork(
        std::move(semaphores));
  }

  mode_ = RepresentationAccessMode::kNone;
}

std::unique_ptr<GrBackendSurfaceMutableState>
SharedImageRepresentationSkiaVkOzone::GetEndAccessState() {
  // There is no layout to change if there is no image.
  if (!vulkan_image_)
    return nullptr;

  const uint32_t kSingleDeviceUsage = SHARED_IMAGE_USAGE_DISPLAY |
                                      SHARED_IMAGE_USAGE_RASTER |
                                      SHARED_IMAGE_USAGE_OOP_RASTERIZATION;

  // If SharedImage is used outside of current VkDeviceQueue we need to transfer
  // image back to it's original queue. Note, that for multithreading we use
  // same vkDevice, so technically we could transfer between queues instead of
  // jumping to external queue. But currently it's not possible because we
  // create new vkImage each time.
  if ((ozone_backing()->usage() & ~kSingleDeviceUsage) ||
      ozone_backing()->is_thread_safe()) {
    return std::make_unique<GrBackendSurfaceMutableState>(
        VK_IMAGE_LAYOUT_UNDEFINED, vulkan_image_->queue_family_index());
  }
  return nullptr;
}

}  // namespace gpu
