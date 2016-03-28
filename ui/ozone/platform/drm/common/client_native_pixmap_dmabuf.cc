// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/common/client_native_pixmap_dmabuf.h"

#include <fcntl.h>
#include <stddef.h>
#include <sys/mman.h>
#include <xf86drm.h>

#include "base/process/memory.h"
#include "base/trace_event/trace_event.h"

namespace ui {

// static
scoped_ptr<ClientNativePixmap> ClientNativePixmapDmaBuf::ImportFromDmabuf(
    int dmabuf_fd,
    const gfx::Size& size,
    int stride) {
  DCHECK_GE(dmabuf_fd, 0);
  return make_scoped_ptr(new ClientNativePixmapDmaBuf(dmabuf_fd, size, stride));
}

ClientNativePixmapDmaBuf::ClientNativePixmapDmaBuf(int dmabuf_fd,
                                                   const gfx::Size& size,
                                                   int stride)
    : size_(size), stride_(stride) {
  TRACE_EVENT0("drm", "ClientNativePixmapDmaBuf");
  size_t map_size = stride_ * size_.height();
  data_ = mmap(nullptr, map_size, (PROT_READ | PROT_WRITE), MAP_SHARED,
               dmabuf_fd, 0);
  CHECK_NE(data_, MAP_FAILED);
}

ClientNativePixmapDmaBuf::~ClientNativePixmapDmaBuf() {
  TRACE_EVENT0("drm", "~ClientNativePixmapDmaBuf");
  size_t size = stride_ * size_.height();
  int ret = munmap(data_, size);
  DCHECK(!ret);
}

void* ClientNativePixmapDmaBuf::Map() {
  return data_;
}

void ClientNativePixmapDmaBuf::Unmap() {}

void ClientNativePixmapDmaBuf::GetStride(int* stride) const {
  *stride = stride_;
}

}  // namespace ui
