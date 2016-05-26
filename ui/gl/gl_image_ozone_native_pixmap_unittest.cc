// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gl/gl_image_ozone_native_pixmap.h"
#include "ui/gl/test/gl_image_test_template.h"
#include "ui/ozone/public/client_native_pixmap.h"
#include "ui/ozone/public/client_native_pixmap_factory.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace gl {
namespace {

template <gfx::BufferUsage usage>
class GLImageOzoneNativePixmapTestDelegate {
 public:
  GLImageOzoneNativePixmapTestDelegate() {
    client_pixmap_factory_ = ui::ClientNativePixmapFactory::Create();
  }
  scoped_refptr<gl::GLImage> CreateSolidColorImage(
      const gfx::Size& size,
      const uint8_t color[4]) const {
    ui::SurfaceFactoryOzone* surface_factory =
        ui::OzonePlatform::GetInstance()->GetSurfaceFactoryOzone();
    scoped_refptr<ui::NativePixmap> pixmap =
        surface_factory->CreateNativePixmap(gfx::kNullAcceleratedWidget, size,
                                            gfx::BufferFormat::BGRA_8888,
                                            usage);
    DCHECK(pixmap);
    if (usage == gfx::BufferUsage::GPU_READ_CPU_READ_WRITE) {
      auto client_pixmap = client_pixmap_factory_->ImportFromHandle(
          pixmap->ExportHandle(), size, usage);
      void* data = client_pixmap->Map();
      EXPECT_TRUE(data);
      GLImageTestSupport::SetBufferDataToColor(
          size.width(), size.height(), pixmap->GetDmaBufPitch(0), 0,
          pixmap->GetBufferFormat(), color, static_cast<uint8_t*>(data));
      client_pixmap->Unmap();
    }

    scoped_refptr<gl::GLImageOzoneNativePixmap> image(
        new gl::GLImageOzoneNativePixmap(size, GL_RGBA));
    EXPECT_TRUE(image->Initialize(pixmap.get(), pixmap->GetBufferFormat()));
    return image;
  }

  unsigned GetTextureTarget() const { return GL_TEXTURE_2D; }

 private:
  std::unique_ptr<ui::ClientNativePixmapFactory> client_pixmap_factory_;
};

INSTANTIATE_TYPED_TEST_CASE_P(
    GLImageOzoneNativePixmap,
    GLImageTest,
    GLImageOzoneNativePixmapTestDelegate<gfx::BufferUsage::SCANOUT>);

// This test is disabled since the trybots are running with Ozone X11
// implementation that doesn't support creating ClientNativePixmap.
// TODO(dcastagna): Implement ClientNativePixmapFactory on Ozone X11.
INSTANTIATE_TYPED_TEST_CASE_P(DISABLED_GLImageOzoneNativePixmap,
                              GLImageBindTest,
                              GLImageOzoneNativePixmapTestDelegate<
                                  gfx::BufferUsage::GPU_READ_CPU_READ_WRITE>);

}  // namespace
}  // namespace gl
