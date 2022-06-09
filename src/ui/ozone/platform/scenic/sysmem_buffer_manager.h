// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_SCENIC_SYSMEM_BUFFER_MANAGER_H_
#define UI_OZONE_PLATFORM_SCENIC_SYSMEM_BUFFER_MANAGER_H_

#include <fuchsia/sysmem/cpp/fidl.h>
#include <lib/ui/scenic/cpp/session.h>
#include <vulkan/vulkan.h>

#include <unordered_map>

#include "base/containers/small_map.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/unguessable_token.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_pixmap_handle.h"

namespace ui {

class SysmemBufferCollection;
class ScenicSurfaceFactory;

class SysmemBufferManager {
 public:
  explicit SysmemBufferManager(ScenicSurfaceFactory* scenic_surface_factory);

  SysmemBufferManager(const SysmemBufferManager&) = delete;
  SysmemBufferManager& operator=(const SysmemBufferManager&) = delete;

  ~SysmemBufferManager();

  // Initializes the buffer manager with a connection to the sysmem service.
  void Initialize(fuchsia::sysmem::AllocatorHandle allocator);

  // Disconnects from the sysmem service. After disconnecting, it's safe to call
  // Initialize() again.
  void Shutdown();

  // Returns sysmem allocator. Should only be called after `Initialize()` and
  // before `Shutdown()`.
  fuchsia::sysmem::Allocator_Sync* GetAllocator();

  scoped_refptr<SysmemBufferCollection> CreateCollection(
      VkDevice vk_device,
      gfx::Size size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      size_t num_buffers);

  scoped_refptr<SysmemBufferCollection> ImportSysmemBufferCollection(
      VkDevice vk_device,
      gfx::SysmemBufferCollectionId id,
      zx::channel token,
      gfx::Size size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      size_t min_buffer_count,
      bool register_with_image_pipe);

  scoped_refptr<SysmemBufferCollection> GetCollectionById(
      gfx::SysmemBufferCollectionId id);

 private:
  void RegisterCollection(SysmemBufferCollection* collection);
  void OnCollectionDestroyed(gfx::SysmemBufferCollectionId id);

  ScenicSurfaceFactory* const scenic_surface_factory_;
  fuchsia::sysmem::AllocatorSyncPtr allocator_;

  base::small_map<std::unordered_map<gfx::SysmemBufferCollectionId,
                                     SysmemBufferCollection*,
                                     base::UnguessableTokenHash>>
      collections_ GUARDED_BY(collections_lock_);
  base::Lock collections_lock_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_SCENIC_SYSMEM_BUFFER_MANAGER_H_
