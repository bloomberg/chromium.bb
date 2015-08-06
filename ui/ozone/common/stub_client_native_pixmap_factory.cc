// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/common/stub_client_native_pixmap_factory.h"

#include "base/file_descriptor_posix.h"

namespace ui {

namespace {

class StubClientNativePixmapFactory : public ClientNativePixmapFactory {
 public:
  StubClientNativePixmapFactory() {}
  ~StubClientNativePixmapFactory() override {}

  // ClientNativePixmapFactory:
  std::vector<Configuration> GetSupportedConfigurations() const override {
    return std::vector<Configuration>();
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
  DISALLOW_COPY_AND_ASSIGN(StubClientNativePixmapFactory);
};

}  // namespace

ClientNativePixmapFactory* CreateStubClientNativePixmapFactory() {
  return new StubClientNativePixmapFactory;
}

}  // namespace ui
