// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/client_native_pixmap.h"
#include "ui/gl/gl_image_native_pixmap.h"
#include "ui/gl/test/gl_image_test_template.h"
#include "ui/ozone/public/client_native_pixmap_factory_ozone.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace gl {
namespace {

const uint8_t kRed[] = {0xF0, 0x0, 0x0, 0xFF};
const uint8_t kGreen[] = {0x0, 0xFF, 0x0, 0xFF};

// These values are picked so that RGB -> YVU on the CPU converted
// back to RGB on the GPU produces the original RGB values without
// any error.
const uint8_t kYvuColor[] = {0x10, 0x20, 0, 0xFF};

template <gfx::BufferUsage usage, gfx::BufferFormat format>
class GLImageNativePixmapTestDelegate {
 public:
  GLImageNativePixmapTestDelegate() {
    client_pixmap_factory_ = ui::CreateClientNativePixmapFactoryOzone();
  }
  scoped_refptr<GLImage> CreateSolidColorImage(const gfx::Size& size,
                                               const uint8_t color[4]) const {
    ui::SurfaceFactoryOzone* surface_factory =
        ui::OzonePlatform::GetInstance()->GetSurfaceFactoryOzone();
    scoped_refptr<gfx::NativePixmap> pixmap =
        surface_factory->CreateNativePixmap(gfx::kNullAcceleratedWidget, size,
                                            format, usage);
    DCHECK(pixmap);
    if (usage == gfx::BufferUsage::GPU_READ_CPU_READ_WRITE ||
        usage == gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE) {
      auto client_pixmap = client_pixmap_factory_->ImportFromHandle(
          pixmap->ExportHandle(), size, usage);
      bool mapped = client_pixmap->Map();
      EXPECT_TRUE(mapped);

      for (size_t plane = 0; plane < NumberOfPlanesForBufferFormat(format);
           ++plane) {
        void* data = client_pixmap->GetMemoryAddress(plane);
        GLImageTestSupport::SetBufferDataToColor(
            size.width(), size.height(), pixmap->GetDmaBufPitch(plane), plane,
            pixmap->GetBufferFormat(), color, static_cast<uint8_t*>(data));
      }
      client_pixmap->Unmap();
    }

    scoped_refptr<gl::GLImageNativePixmap> image(new gl::GLImageNativePixmap(
        size, gl::GLImageNativePixmap::GetInternalFormatForTesting(format)));
    EXPECT_TRUE(image->Initialize(pixmap.get(), pixmap->GetBufferFormat()));
    return image;
  }

  unsigned GetTextureTarget() const { return GL_TEXTURE_EXTERNAL_OES; }

  const uint8_t* GetImageColor() {
    if (format == gfx::BufferFormat::R_8) {
      return kRed;
    } else if (format == gfx::BufferFormat::YVU_420 ||
               format == gfx::BufferFormat::YUV_420_BIPLANAR) {
      return kYvuColor;
    }
    return kGreen;
  }

 private:
  std::unique_ptr<gfx::ClientNativePixmapFactory> client_pixmap_factory_;
};

using GLImageScanoutType = testing::Types<
    GLImageNativePixmapTestDelegate<gfx::BufferUsage::SCANOUT,
                                    gfx::BufferFormat::BGRA_8888>>;

INSTANTIATE_TYPED_TEST_CASE_P(GLImageNativePixmapScanout,
                              GLImageTest,
                              GLImageScanoutType);

using GLImageReadWriteType = testing::Types<
    GLImageNativePixmapTestDelegate<gfx::BufferUsage::GPU_READ_CPU_READ_WRITE,
                                    gfx::BufferFormat::R_8>,
    GLImageNativePixmapTestDelegate<gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE,
                                    gfx::BufferFormat::YUV_420_BIPLANAR>>;

using GLImageBindTestTypes = testing::Types<
    GLImageNativePixmapTestDelegate<gfx::BufferUsage::GPU_READ_CPU_READ_WRITE,
                                    gfx::BufferFormat::BGRA_8888>,
    GLImageNativePixmapTestDelegate<gfx::BufferUsage::GPU_READ_CPU_READ_WRITE,
                                    gfx::BufferFormat::R_8>,
    GLImageNativePixmapTestDelegate<gfx::BufferUsage::GPU_READ_CPU_READ_WRITE,
                                    gfx::BufferFormat::YVU_420>,
    GLImageNativePixmapTestDelegate<gfx::BufferUsage::GPU_READ_CPU_READ_WRITE,
                                    gfx::BufferFormat::YUV_420_BIPLANAR>,
    GLImageNativePixmapTestDelegate<gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE,
                                    gfx::BufferFormat::YUV_420_BIPLANAR>>;

// These tests are disabled since the trybots are running with Ozone X11
// implementation that doesn't support creating ClientNativePixmap.
// TODO(dcastagna): Implement ClientNativePixmapFactory on Ozone X11.
INSTANTIATE_TYPED_TEST_CASE_P(DISABLED_GLImageNativePixmapReadWrite,
                              GLImageTest,
                              GLImageReadWriteType);

INSTANTIATE_TYPED_TEST_CASE_P(DISABLED_GLImageNativePixmap,
                              GLImageBindTest,
                              GLImageBindTestTypes);

}  // namespace
}  // namespace gl
