// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/common/client_native_pixmap_factory_gbm.h"

#include "base/file_descriptor_posix.h"
#include "ui/gfx/native_pixmap_handle_ozone.h"
#include "ui/ozone/public/client_native_pixmap_factory.h"

#if defined(USE_VGEM_MAP)
#include <fcntl.h>
#include "ui/ozone/platform/drm/gpu/client_native_pixmap_vgem.h"
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

class ClientNativePixmapFactoryGbm : public ClientNativePixmapFactory {
 public:
  ClientNativePixmapFactoryGbm() {
#if defined(USE_VGEM_MAP)
    // TODO(dshwang): remove ad-hoc file open. crrev.com/1248713002
    static const char kVgemPath[] = "/dev/dri/renderD129";
    int vgem_fd = open(kVgemPath, O_RDWR | O_CLOEXEC);
    vgem_fd_.reset(vgem_fd);
    DCHECK_GE(vgem_fd_.get(), 0) << "Failed to open: " << kVgemPath;
#endif
  }
  ~ClientNativePixmapFactoryGbm() override {}

  // ClientNativePixmapFactory:
  std::vector<Configuration> GetSupportedConfigurations() const override {
    const Configuration kConfigurations[] = {
        {gfx::BufferFormat::BGRA_8888, gfx::BufferUsage::SCANOUT},
        {gfx::BufferFormat::BGRX_8888, gfx::BufferUsage::SCANOUT}};
    std::vector<Configuration> configurations(
        kConfigurations, kConfigurations + arraysize(kConfigurations));
#if defined(USE_VGEM_MAP)
    configurations.push_back(
        {gfx::BufferFormat::BGRA_8888, gfx::BufferUsage::MAP});
#endif
    return configurations;
  }
  scoped_ptr<ClientNativePixmap> ImportFromHandle(
      const gfx::NativePixmapHandle& handle,
      const gfx::Size& size,
      gfx::BufferUsage usage) override {
    base::ScopedFD scoped_fd(handle.fd.fd);

    switch (usage) {
      case gfx::BufferUsage::MAP:
#if defined(USE_VGEM_MAP)
        return ClientNativePixmapVgem::ImportFromDmabuf(
            vgem_fd_.get(), scoped_fd.get(), size, handle.stride);
#endif
        NOTREACHED();
        return nullptr;
      case gfx::BufferUsage::PERSISTENT_MAP:
        NOTREACHED();
        return nullptr;
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

}  // namespace

ClientNativePixmapFactory* CreateClientNativePixmapFactoryGbm() {
  return new ClientNativePixmapFactoryGbm();
}

}  // namespace ui
