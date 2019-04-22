// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/external_vk_image_skia_representation.h"

#include <limits>
#include <utility>

#include "base/trace_event/trace_event.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_util.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"

namespace gpu {

ExternalVkImageSkiaRepresentation::ExternalVkImageSkiaRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker)
    : SharedImageRepresentationSkia(manager, backing, tracker) {
}

ExternalVkImageSkiaRepresentation::~ExternalVkImageSkiaRepresentation() {
}

sk_sp<SkSurface> ExternalVkImageSkiaRepresentation::BeginWriteAccess(
    int final_msaa_count,
    const SkSurfaceProps& surface_props) {
  DCHECK_EQ(access_mode_, kNone) << "Previous access hasn't ended yet";
  DCHECK(!surface_);

  auto promise_texture = BeginAccess(false /* readonly */);
  if (!promise_texture)
    return nullptr;
  SkColorType sk_color_type = viz::ResourceFormatToClosestSkColorType(
      true /* gpu_compositing */, format());
  surface_ = SkSurface::MakeFromBackendTextureAsRenderTarget(
      backing_impl()->context_state()->gr_context(),
      promise_texture->backendTexture(), kTopLeft_GrSurfaceOrigin,
      final_msaa_count, sk_color_type,
      backing_impl()->color_space().ToSkColorSpace(), &surface_props);
  access_mode_ = kWrite;
  return surface_;
}

void ExternalVkImageSkiaRepresentation::EndWriteAccess(
    sk_sp<SkSurface> surface) {
  DCHECK_EQ(access_mode_, kWrite)
      << "EndWriteAccess is called before BeginWriteAccess";
  DCHECK(surface_);
  surface_ = nullptr;
  EndAccess(false /* readonly */);
  access_mode_ = kNone;
}

sk_sp<SkPromiseImageTexture>
ExternalVkImageSkiaRepresentation::BeginReadAccess() {
  DCHECK_EQ(access_mode_, kNone) << "Previous access hasn't ended yet";
  DCHECK(!surface_);

  auto promise_texture = BeginAccess(true /* readonly */);
  if (!promise_texture)
    return nullptr;
  access_mode_ = kRead;
  return promise_texture;
}

void ExternalVkImageSkiaRepresentation::EndReadAccess() {
  DCHECK_EQ(access_mode_, kRead)
      << "EndReadAccess is called before BeginReadAccess";
  EndAccess(true /* readonly */);
  access_mode_ = kNone;
}

sk_sp<SkPromiseImageTexture> ExternalVkImageSkiaRepresentation::BeginAccess(
    bool readonly) {
  DCHECK_EQ(access_mode_, kNone);

  std::vector<SemaphoreHandle> handles;
  if (!backing_impl()->BeginAccess(readonly, &handles))
    return nullptr;

  std::vector<VkSemaphore> begin_access_semaphores;
  for (auto& handle : handles) {
    VkSemaphore semaphore = vk_implementation()->ImportSemaphoreHandle(
        vk_device(), std::move(handle));
    if (semaphore != VK_NULL_HANDLE)
      begin_access_semaphores.push_back(semaphore);
  }

  if (!begin_access_semaphores.empty()) {
    // Submit wait semaphore to the queue. Note that Skia uses the same queue
    // exposed by vk_queue(), so this will work due to Vulkan queue ordering.
    if (!SubmitWaitVkSemaphores(vk_queue(), begin_access_semaphores)) {
      LOG(ERROR) << "Failed to wait on semaphore";
      // Since the semaphore was not actually sent to the queue, it is safe to
      // destroy the |begin_access_semaphores| here.
      DestroySemaphoresImmediate(std::move(begin_access_semaphores));
      return nullptr;
    }

    // Enqueue delayed cleanup of the semaphores.
    fence_helper()->EnqueueSemaphoresCleanupForSubmittedWork(
        std::move(begin_access_semaphores));
  }

  // Create backend texture from the VkImage.
  GrVkAlloc alloc(backing_impl()->memory(), 0 /* offset */,
                  backing_impl()->memory_size(), 0 /* flags */);
  GrVkImageInfo vk_image_info(backing_impl()->image(), alloc,
                              VK_IMAGE_TILING_OPTIMAL,
                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                              backing_impl()->vk_format(), 1 /* levelCount */);

  return SkPromiseImageTexture::Make(
      GrBackendTexture(size().width(), size().height(), vk_image_info));
}

void ExternalVkImageSkiaRepresentation::EndAccess(bool readonly) {
  DCHECK_NE(access_mode_, kNone);

  VkSemaphore end_access_semaphore =
      vk_implementation()->CreateExternalSemaphore(backing_impl()->device());
  VkFence end_access_fence = VK_NULL_HANDLE;
  if (end_access_semaphore != VK_NULL_HANDLE) {
    if (VK_SUCCESS != fence_helper()->GetFence(&end_access_fence)) {
      // Since the semaphore was not actually sent to the queue, it is safe to
      // destroy the |end_access_semaphore| here.
      DestroySemaphoreImmediate(end_access_semaphore);
    }
    // Submit wait semaphore to the queue. Note that Skia uses the same queue
    // exposed by vk_queue(), so this will work due to Vulkan queue ordering.
    if (!SubmitSignalVkSemaphore(vk_queue(), end_access_semaphore,
                                 end_access_fence)) {
      LOG(ERROR) << "Failed to wait on semaphore";
      // Since the semaphore was not actually sent to the queue, it is safe to
      // destroy the |end_access_semaphore| here.
      DestroySemaphoreImmediate(end_access_semaphore);
      // Same for the fence.
      vkDestroyFence(vk_device(), end_access_fence, nullptr);
      end_access_fence = VK_NULL_HANDLE;
      end_access_semaphore = VK_NULL_HANDLE;
    }
  }

  SemaphoreHandle handle;
  if (end_access_semaphore != VK_NULL_HANDLE) {
    handle = vk_implementation()->GetSemaphoreHandle(vk_device(),
                                                     end_access_semaphore);
    if (!handle.is_valid())
      LOG(FATAL) << "Failed to get handle from a semaphore.";

    // We're done with the semaphore, enqueue deferred cleanup.
    fence_helper()->EnqueueSemaphoreCleanupForSubmittedWork(
        end_access_semaphore);
    fence_helper()->EnqueueFence(end_access_fence);
  }
  backing_impl()->EndAccess(readonly, std::move(handle));
}

void ExternalVkImageSkiaRepresentation::DestroySemaphoresImmediate(
    std::vector<VkSemaphore> semaphores) {
  if (semaphores.empty())
    return;
  for (VkSemaphore semaphore : semaphores)
    vkDestroySemaphore(vk_device(), semaphore, nullptr /* pAllocator */);
}

void ExternalVkImageSkiaRepresentation::DestroySemaphoreImmediate(
    VkSemaphore semaphore) {
  if (semaphore == VK_NULL_HANDLE)
    return;
  return DestroySemaphoresImmediate({semaphore});
}

}  // namespace gpu
