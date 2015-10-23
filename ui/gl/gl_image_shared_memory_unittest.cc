// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/shared_memory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_image_shared_memory.h"
#include "ui/gl/test/gl_image_test_template.h"

namespace gfx {
namespace {

template <BufferFormat format>
class GLImageSharedMemoryTestDelegate {
 public:
  scoped_refptr<GLImage> CreateSolidColorImage(const Size& size,
                                               const uint8_t color[4]) const {
    DCHECK_EQ(NumberOfPlanesForBufferFormat(format), 1u);
    base::SharedMemory shared_memory;
    bool rv = shared_memory.CreateAndMapAnonymous(
        BufferSizeForBufferFormat(size, format));
    DCHECK(rv);
    GLImageTestSupport::SetBufferDataToColor(
        size.width(), size.height(),
        static_cast<int>(RowSizeForBufferFormat(size.width(), format, 0)),
        format, color, reinterpret_cast<uint8_t*>(shared_memory.memory()));
    scoped_refptr<GLImageSharedMemory> image(new GLImageSharedMemory(
        size, GLImageMemory::GetInternalFormatForTesting(format)));
    rv = image->Initialize(
        base::SharedMemory::DuplicateHandle(shared_memory.handle()),
        GenericSharedMemoryId(0), format);
    EXPECT_TRUE(rv);
    return image;
  }
};

using GLImageTestTypes =
    testing::Types<GLImageSharedMemoryTestDelegate<BufferFormat::RGBX_8888>,
                   GLImageSharedMemoryTestDelegate<BufferFormat::RGBA_8888>,
                   GLImageSharedMemoryTestDelegate<BufferFormat::BGRX_8888>,
                   GLImageSharedMemoryTestDelegate<BufferFormat::BGRA_8888>>;

INSTANTIATE_TYPED_TEST_CASE_P(GLImageSharedMemory,
                              GLImageTest,
                              GLImageTestTypes);

INSTANTIATE_TYPED_TEST_CASE_P(GLImageSharedMemory,
                              GLImageCopyTest,
                              GLImageTestTypes);

}  // namespace
}  // namespace gfx
