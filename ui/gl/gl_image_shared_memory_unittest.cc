// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/shared_memory.h"
#include "base/sys_info.h"
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
        GenericSharedMemoryId(0), format, 0);
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

class GLImageSharedMemoryPoolTestDelegate {
 public:
  scoped_refptr<GLImage> CreateSolidColorImage(const Size& size,
                                               const uint8_t color[4]) const {
    // Create a shared memory segment that is 2 pages larger than image.
    size_t pool_size =
        BufferSizeForBufferFormat(size, BufferFormat::RGBA_8888) +
        base::SysInfo::VMAllocationGranularity() * 3;
    base::SharedMemory shared_memory;
    bool rv = shared_memory.CreateAndMapAnonymous(pool_size);
    DCHECK(rv);
    // Initialize memory to a value that is easy to recognize if test fails.
    memset(shared_memory.memory(), 0x55, pool_size);
    // Place buffer at a non-zero non-page-aligned offset in shared memory.
    size_t buffer_offset = 3 * base::SysInfo::VMAllocationGranularity() / 2;
    GLImageTestSupport::SetBufferDataToColor(
        size.width(), size.height(),
        static_cast<int>(
            RowSizeForBufferFormat(size.width(), BufferFormat::RGBA_8888, 0)),
        BufferFormat::RGBA_8888, color,
        reinterpret_cast<uint8_t*>(shared_memory.memory()) + buffer_offset);
    scoped_refptr<GLImageSharedMemory> image(
        new GLImageSharedMemory(size, GL_RGBA));
    rv = image->Initialize(
        base::SharedMemory::DuplicateHandle(shared_memory.handle()),
        GenericSharedMemoryId(0), BufferFormat::RGBA_8888, buffer_offset);
    EXPECT_TRUE(rv);
    return image;
  }
};

INSTANTIATE_TYPED_TEST_CASE_P(GLImageSharedMemoryPool,
                              GLImageCopyTest,
                              GLImageSharedMemoryPoolTestDelegate);

}  // namespace
}  // namespace gfx
