// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_representation_dawn_ozone.h"

#include <dawn_native/VulkanBackend.h>

#include <vulkan/vulkan.h>
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_pixmap.h"

namespace gpu {

SharedImageRepresentationDawnOzone::SharedImageRepresentationDawnOzone(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    WGPUDevice device,
    WGPUTextureFormat format,
    scoped_refptr<gfx::NativePixmap> pixmap,
    scoped_refptr<base::RefCountedData<DawnProcTable>> dawn_procs)
    : SharedImageRepresentationDawn(manager, backing, tracker),
      device_(device),
      format_(format),
      pixmap_(pixmap),
      dawn_procs_(dawn_procs) {
  DCHECK(device_);

  // Keep a reference to the device so that it stays valid (it might become
  // lost in which case operations will be noops).
  dawn_procs_->data.deviceReference(device_);
}

SharedImageRepresentationDawnOzone::~SharedImageRepresentationDawnOzone() {
  EndAccess();
  dawn_procs_->data.deviceRelease(device_);
}

WGPUTexture SharedImageRepresentationDawnOzone::BeginAccess(
    WGPUTextureUsage usage) {
  // It doesn't make sense to have two overlapping BeginAccess calls on the same
  // representation.
  if (texture_) {
    return nullptr;
  }
  if (!ozone_backing()->VaSync()) {
    return nullptr;
  }
  DCHECK(pixmap_->GetNumberOfPlanes() == 1)
      << "Multi-plane formats are not supported.";
  // TODO(hob): Synchronize access to the dma-buf by waiting on all semaphores
  // tracked by SharedImageBackingOzone.
  gfx::Size pixmap_size = pixmap_->GetBufferSize();
  WGPUTextureDescriptor texture_descriptor = {};
  texture_descriptor.nextInChain = nullptr;
  texture_descriptor.format = format_;
  texture_descriptor.usage = usage;
  texture_descriptor.dimension = WGPUTextureDimension_2D;
  texture_descriptor.size = {pixmap_size.width(), pixmap_size.height(), 1};
  texture_descriptor.arrayLayerCount = 1;
  texture_descriptor.mipLevelCount = 1;
  texture_descriptor.sampleCount = 1;

  dawn_native::vulkan::ExternalImageDescriptorDmaBuf descriptor = {};
  descriptor.cTextureDescriptor = &texture_descriptor;
  descriptor.isCleared = IsCleared();
  // Import the dma-buf into Dawn via the Vulkan backend. As per the Vulkan
  // documentation, importing memory from a file descriptor transfers
  // ownership of the fd from the application to the Vulkan implementation.
  // Thus, we need to dup the fd so the fd corresponding to the dmabuf isn't
  // closed twice (once by ScopedFD and once by the Vulkan implementation).
  int fd = dup(pixmap_->GetDmaBufFd(0));
  descriptor.memoryFD = fd;
  descriptor.stride = pixmap_->GetDmaBufPitch(0);
  descriptor.drmModifier = pixmap_->GetBufferFormatModifier();
  descriptor.waitFDs = {};

  texture_ = dawn_native::vulkan::WrapVulkanImage(device_, &descriptor);
  if (texture_) {
    // Keep a reference to the texture so that it stays valid (its content
    // might be destroyed).
    dawn_procs_->data.textureReference(texture_);
  } else {
    close(fd);
  }

  return texture_;
}

void SharedImageRepresentationDawnOzone::EndAccess() {
  if (!texture_) {
    return;
  }

  if (dawn_native::IsTextureSubresourceInitialized(texture_, 0, 1, 0, 1)) {
    SetCleared();
  }

  // TODO(hob): Synchronize access to the dma-buf by exporting the VkSemaphore
  // from the WebGPU texture.
  dawn_procs_->data.textureDestroy(texture_);
  dawn_procs_->data.textureRelease(texture_);
  texture_ = nullptr;
}

}  // namespace gpu
