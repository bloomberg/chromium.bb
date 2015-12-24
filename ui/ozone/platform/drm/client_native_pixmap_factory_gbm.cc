// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/client_native_pixmap_factory_gbm.h"

#include <utility>

#include "base/macros.h"
#include "ui/gfx/native_pixmap_handle_ozone.h"
#include "ui/ozone/public/client_native_pixmap_factory.h"  // nogncheck

#if defined(USE_VGEM_MAP)
#include "ui/ozone/platform/drm/common/client_native_pixmap_vgem.h"
#endif

namespace ui {

namespace {

class ClientNativePixmapGbm : public ClientNativePixmap {
 public:
  ClientNativePixmapGbm() {}
  ~ClientNativePixmapGbm() override {}

  void* Map() override {
    NOTREACHED();
    return nullptr;
  }
  void Unmap() override { NOTREACHED(); }
  void GetStride(int* stride) const override { NOTREACHED(); }
};

}  // namespace

class ClientNativePixmapFactoryGbm : public ClientNativePixmapFactory {
 public:
  ClientNativePixmapFactoryGbm() {}
  ~ClientNativePixmapFactoryGbm() override {}

  // ClientNativePixmapFactory:
  void Initialize(base::ScopedFD device_fd) override {
#if defined(USE_VGEM_MAP)
    // It's called in IO thread. We rely on clients for thread-safety.
    // Switching to an IPC message filter ensures thread-safety.
    DCHECK_LT(vgem_fd_.get(), 0);
    vgem_fd_ = std::move(device_fd);
#endif
  }
  bool IsConfigurationSupported(gfx::BufferFormat format,
                                gfx::BufferUsage usage) const override {
    switch (usage) {
      case gfx::BufferUsage::GPU_READ:
      case gfx::BufferUsage::SCANOUT:
        return format == gfx::BufferFormat::RGBA_8888 ||
               format == gfx::BufferFormat::RGBX_8888 ||
               format == gfx::BufferFormat::BGRA_8888 ||
               format == gfx::BufferFormat::BGRX_8888;
      case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
      case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE_PERSISTENT: {
#if defined(USE_VGEM_MAP)
        return vgem_fd_.is_valid() && format == gfx::BufferFormat::BGRA_8888;
#else
        return false;
#endif
      }
    }
    NOTREACHED();
    return false;
  }
  scoped_ptr<ClientNativePixmap> ImportFromHandle(
      const gfx::NativePixmapHandle& handle,
      const gfx::Size& size,
      gfx::BufferUsage usage) override {
    base::ScopedFD scoped_fd(handle.fd.fd);

    switch (usage) {
      case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
      case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE_PERSISTENT:
#if defined(USE_VGEM_MAP)
        // A valid |vgem_fd_| is required to acquire a VGEM bo. |vgem_fd_| is
        // set before a widget is created.
        DCHECK_GE(vgem_fd_.get(), 0);
        return ClientNativePixmapVgem::ImportFromDmabuf(
            vgem_fd_.get(), scoped_fd.get(), size, handle.stride);
#endif
        NOTREACHED();
        return nullptr;
      case gfx::BufferUsage::GPU_READ:
      case gfx::BufferUsage::SCANOUT:
        return make_scoped_ptr<ClientNativePixmapGbm>(
            new ClientNativePixmapGbm);
    }
    NOTREACHED();
    return nullptr;
  }

 private:
#if defined(USE_VGEM_MAP)
  base::ScopedFD vgem_fd_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ClientNativePixmapFactoryGbm);
};

ClientNativePixmapFactory* CreateClientNativePixmapFactoryGbm() {
  return new ClientNativePixmapFactoryGbm();
}

}  // namespace ui
