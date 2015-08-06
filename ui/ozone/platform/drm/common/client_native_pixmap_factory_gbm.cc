// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/common/client_native_pixmap_factory_gbm.h"

#include "base/file_descriptor_posix.h"
#include "ui/ozone/public/client_native_pixmap_factory.h"

namespace ui {

namespace {

class ClientNativePixmapFactoryGbm : public ClientNativePixmapFactory {
 public:
  ClientNativePixmapFactoryGbm() {}
  ~ClientNativePixmapFactoryGbm() override {}

  // ClientNativePixmapFactory:
  std::vector<Configuration> GetSupportedConfigurations() const override {
    const Configuration kConfiguratioins[] = {
        {gfx::BufferFormat::BGRA_8888, gfx::BufferUsage::SCANOUT},
        {gfx::BufferFormat::RGBX_8888, gfx::BufferUsage::SCANOUT}};
    std::vector<Configuration> configurations(
        kConfiguratioins, kConfiguratioins + arraysize(kConfiguratioins));
    return configurations;
  }
  scoped_ptr<ClientNativePixmap> ImportNativePixmap(
      const base::FileDescriptor& handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage) override {
    NOTIMPLEMENTED();
    return nullptr;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ClientNativePixmapFactoryGbm);
};

}  // namespace

ClientNativePixmapFactory* CreateClientNativePixmapFactoryGbm() {
  return new ClientNativePixmapFactoryGbm();
}

}  // namespace ui
