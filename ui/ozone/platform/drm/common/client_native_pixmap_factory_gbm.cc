// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/common/client_native_pixmap_factory_gbm.h"

#include "base/file_descriptor_posix.h"
#include "ui/gfx/native_pixmap_handle_ozone.h"
#include "ui/ozone/public/client_native_pixmap_factory.h"

namespace ui {

namespace {

class ClientNativePixmapGbm : public ClientNativePixmap {
 public:
  ClientNativePixmapGbm() {}
  ~ClientNativePixmapGbm() override {}

  bool Map(void** data) override {
    NOTREACHED();
    return false;
  }
  void Unmap() override { NOTREACHED(); }
  void GetStride(int* stride) const override { NOTREACHED(); }
};

class ClientNativePixmapFactoryGbm : public ClientNativePixmapFactory {
 public:
  ClientNativePixmapFactoryGbm() {}
  ~ClientNativePixmapFactoryGbm() override {}

  // ClientNativePixmapFactory:
  std::vector<Configuration> GetSupportedConfigurations() const override {
    const Configuration kConfigurations[] = {
        {gfx::BufferFormat::BGRA_8888, gfx::BufferUsage::SCANOUT},
        {gfx::BufferFormat::BGRX_8888, gfx::BufferUsage::SCANOUT}};
    std::vector<Configuration> configurations(
        kConfigurations, kConfigurations + arraysize(kConfigurations));
    return configurations;
  }
  scoped_ptr<ClientNativePixmap> ImportFromHandle(
      const gfx::NativePixmapHandle& handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage) override {
    base::ScopedFD scoped_fd(handle.fd.fd);
    return make_scoped_ptr<ClientNativePixmapGbm>(new ClientNativePixmapGbm);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ClientNativePixmapFactoryGbm);
};

}  // namespace

ClientNativePixmapFactory* CreateClientNativePixmapFactoryGbm() {
  return new ClientNativePixmapFactoryGbm();
}

}  // namespace ui
