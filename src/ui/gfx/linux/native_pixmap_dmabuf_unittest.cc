// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/linux/native_pixmap_dmabuf.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/buffer_format_util.h"

namespace gfx {

class NativePixmapDmaBufTest
    : public ::testing::TestWithParam<gfx::BufferFormat> {
 protected:
  gfx::NativePixmapHandle CreateMockNativePixmapHandle(
      gfx::Size image_size,
      const gfx::BufferFormat format) {
    gfx::NativePixmapHandle handle;
    const int num_planes = gfx::NumberOfPlanesForBufferFormat(format);
    for (int i = 0; i < num_planes; ++i) {
      // These values are arbitrarily chosen to be different from each other.
      const int stride = (i + 1) * image_size.width();
      const int offset = i * image_size.width() * image_size.height();
      const uint64_t size = stride * image_size.height();
      const uint64_t modifiers = 1 << i;

      handle.fds.emplace_back(
          base::FileDescriptor(i + 1, true /* auto_close */));

      handle.planes.emplace_back(stride, offset, size, modifiers);
    }

    return handle;
  }
};

INSTANTIATE_TEST_SUITE_P(ConvertTest,
                         NativePixmapDmaBufTest,
                         ::testing::Values(gfx::BufferFormat::RGBX_8888,
                                           gfx::BufferFormat::YVU_420));

// Verifies NativePixmapDmaBuf conversion from and to NativePixmapHandle.
TEST_P(NativePixmapDmaBufTest, Convert) {
  const gfx::BufferFormat format = GetParam();
  const gfx::Size image_size(128, 64);

  gfx::NativePixmapHandle origin_handle =
      CreateMockNativePixmapHandle(image_size, format);

  // NativePixmapHandle to NativePixmapDmabuf
  scoped_refptr<gfx::NativePixmap> native_pixmap_dmabuf(
      new gfx::NativePixmapDmaBuf(image_size, format, origin_handle));
  EXPECT_TRUE(native_pixmap_dmabuf->AreDmaBufFdsValid());

  // NativePixmap to NativePixmapHandle.
  gfx::NativePixmapHandle handle;
  const size_t num_planes = gfx::NumberOfPlanesForBufferFormat(
      native_pixmap_dmabuf->GetBufferFormat());
  for (size_t i = 0; i < num_planes; ++i) {
    handle.fds.emplace_back(base::FileDescriptor(
        native_pixmap_dmabuf->GetDmaBufFd(i), true /* auto_close */));

    handle.planes.emplace_back(
        native_pixmap_dmabuf->GetDmaBufPitch(i),
        native_pixmap_dmabuf->GetDmaBufOffset(i),
        native_pixmap_dmabuf->GetDmaBufPitch(i) * image_size.height(),
        native_pixmap_dmabuf->GetDmaBufModifier(i));
  }

  // NativePixmapHandle is unchanged during convertion to NativePixmapDmabuf.
  EXPECT_EQ(origin_handle.fds, handle.fds);
  EXPECT_EQ(origin_handle.fds.size(), handle.planes.size());
  EXPECT_EQ(origin_handle.planes.size(), handle.planes.size());
  for (size_t i = 0; i < origin_handle.planes.size(); ++i) {
    EXPECT_EQ(origin_handle.planes[i].stride, handle.planes[i].stride);
    EXPECT_EQ(origin_handle.planes[i].offset, handle.planes[i].offset);
    EXPECT_EQ(origin_handle.planes[i].size, handle.planes[i].size);
    EXPECT_EQ(origin_handle.planes[i].modifier, handle.planes[i].modifier);
  }
}

}  // namespace gfx
