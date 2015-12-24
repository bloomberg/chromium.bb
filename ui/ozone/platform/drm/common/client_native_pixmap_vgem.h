// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_COMMON_CLIENT_NATIVE_PIXMAP_VGEM_H_
#define UI_OZONE_PLATFORM_DRM_COMMON_CLIENT_NATIVE_PIXMAP_VGEM_H_

#include <stdint.h>

#include "base/file_descriptor_posix.h"
#include "base/macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_pixmap_handle_ozone.h"
#include "ui/ozone/public/client_native_pixmap.h"

namespace ui {

class ClientNativePixmapVgem : public ClientNativePixmap {
 public:
  static scoped_ptr<ClientNativePixmap> ImportFromDmabuf(int vgem_fd,
                                                         int dmabuf_fd,
                                                         const gfx::Size& size,
                                                         int stride);

  ~ClientNativePixmapVgem() override;

  // Overridden from ClientNativePixmap.
  void* Map() override;
  void Unmap() override;
  void GetStride(int* stride) const override;

 private:
  ClientNativePixmapVgem(int vgem_fd,
                         uint32_t vgem_bo_handle,
                         const gfx::Size& size,
                         int stride);

  const int vgem_fd_;
  const uint32_t vgem_bo_handle_;
  const gfx::Size size_;
  const int stride_;
  void* data_;

  DISALLOW_COPY_AND_ASSIGN(ClientNativePixmapVgem);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_COMMON_CLIENT_NATIVE_PIXMAP_VGEM_H_
