// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/common/client_native_pixmap_vgem.h"

#include <fcntl.h>
#include <stddef.h>
#include <sys/mman.h>
#include <vgem_drm.h>
#include <xf86drm.h>

#include "base/process/memory.h"
#include "base/trace_event/trace_event.h"

namespace ui {

// static
scoped_ptr<ClientNativePixmap> ClientNativePixmapVgem::ImportFromDmabuf(
    int vgem_fd,
    int dmabuf_fd,
    const gfx::Size& size,
    int stride) {
  DCHECK_GE(vgem_fd, 0);
  DCHECK_GE(dmabuf_fd, 0);
  uint32_t vgem_bo_handle = 0;
  int ret = drmPrimeFDToHandle(vgem_fd, dmabuf_fd, &vgem_bo_handle);
  DCHECK(!ret) << "drmPrimeFDToHandle failed.";
  return make_scoped_ptr(
      new ClientNativePixmapVgem(vgem_fd, vgem_bo_handle, size, stride));
}

ClientNativePixmapVgem::ClientNativePixmapVgem(int vgem_fd,
                                               uint32_t vgem_bo_handle,
                                               const gfx::Size& size,
                                               int stride)
    : vgem_fd_(vgem_fd),
      vgem_bo_handle_(vgem_bo_handle),
      size_(size),
      stride_(stride),
      data_(nullptr) {
  DCHECK(vgem_bo_handle_);
  struct drm_mode_map_dumb mmap_arg = {0};
  mmap_arg.handle = vgem_bo_handle_;
  size_t map_size = stride_ * size_.height();
  int ret = drmIoctl(vgem_fd_, DRM_IOCTL_VGEM_MODE_MAP_DUMB, &mmap_arg);
  if (ret) {
    PLOG(ERROR) << "fail to map a vgem buffer.";
    base::TerminateBecauseOutOfMemory(map_size);
  }
  DCHECK(mmap_arg.offset);

  data_ = mmap(nullptr, map_size, (PROT_READ | PROT_WRITE), MAP_SHARED,
               vgem_fd_, mmap_arg.offset);
  DCHECK_NE(data_, MAP_FAILED);
}

ClientNativePixmapVgem::~ClientNativePixmapVgem() {
  size_t size = stride_ * size_.height();
  int ret = munmap(data_, size);
  DCHECK(!ret) << "fail to munmap a vgem buffer.";

  struct drm_gem_close close = {0};
  close.handle = vgem_bo_handle_;
  ret = drmIoctl(vgem_fd_, DRM_IOCTL_GEM_CLOSE, &close);
  DCHECK(!ret) << "fail to free a vgem buffer.";
}

void* ClientNativePixmapVgem::Map() {
  return data_;
}

void ClientNativePixmapVgem::Unmap() {}

void ClientNativePixmapVgem::GetStride(int* stride) const {
  *stride = stride_;
}

}  // namespace ui
