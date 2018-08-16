// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/client_native_pixmap_factory_wayland.h"

#include "ui/gfx/linux/client_native_pixmap_dmabuf.h"
#include "ui/gfx/linux/client_native_pixmap_factory_dmabuf.h"
#include "ui/ozone/public/ozone_platform.h"

namespace ui {

// Implements ClientNativePixmapFactory to provide a more accurate buffer format
// support when Wayland dmabuf is used.
class ClientNativePixmapFactoryWayland : public gfx::ClientNativePixmapFactory {
 public:
  ClientNativePixmapFactoryWayland() {
    dmabuf_factory_.reset(gfx::CreateClientNativePixmapFactoryDmabuf());
  }
  ~ClientNativePixmapFactoryWayland() override {}

  // ClientNativePixmapFactory overrides:
  bool IsConfigurationSupported(gfx::BufferFormat format,
                                gfx::BufferUsage usage) const override {
    OzonePlatform::PlatformProperties properties =
        OzonePlatform::GetInstance()->GetPlatformProperties();
    if (properties.supported_buffer_formats.empty()) {
      // If the compositor did not announce supported buffer formats, do our
      // best and assume those are supported.
      return dmabuf_factory_->IsConfigurationSupported(format, usage);
    }

    for (auto buffer_format : properties.supported_buffer_formats) {
      if (buffer_format == format)
        return dmabuf_factory_->IsConfigurationSupported(format, usage);
    }
    return false;
  }

  std::unique_ptr<gfx::ClientNativePixmap> ImportFromHandle(
      const gfx::NativePixmapHandle& handle,
      const gfx::Size& size,
      gfx::BufferUsage usage) override {
    return dmabuf_factory_->ImportFromHandle(handle, size, usage);
  }

 private:
  std::unique_ptr<ClientNativePixmapFactory> dmabuf_factory_;
  DISALLOW_COPY_AND_ASSIGN(ClientNativePixmapFactoryWayland);
};

gfx::ClientNativePixmapFactory* CreateClientNativePixmapFactoryWayland() {
  return new ClientNativePixmapFactoryWayland();
}

}  // namespace ui
