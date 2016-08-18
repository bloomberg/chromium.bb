// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/scoped_file.h"
#include "build/build_config.h"
#include "services/ui/common/gpu_type_converters.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

// Test for mojo TypeConverter of ui::mojom::GpuMemoryBufferHandle
TEST(MusGpuTypeConvertersTest, GpuMemoryBufferHandle) {
  const gfx::GpuMemoryBufferId kId(99);
  const uint32_t kOffset = 126;
  const int32_t kStride = 256;
  base::SharedMemory shared_memory;
  ASSERT_TRUE(shared_memory.CreateAnonymous(1024));
  ASSERT_TRUE(shared_memory.Map(1024));

  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::SHARED_MEMORY_BUFFER;
  handle.id = kId;
  handle.handle = base::SharedMemory::DuplicateHandle(shared_memory.handle());
  handle.offset = kOffset;
  handle.stride = kStride;

  ui::mojom::GpuMemoryBufferHandlePtr gpu_handle =
      ui::mojom::GpuMemoryBufferHandle::From<gfx::GpuMemoryBufferHandle>(
          handle);
  ASSERT_EQ(gpu_handle->type, ui::mojom::GpuMemoryBufferType::SHARED_MEMORY);
  ASSERT_EQ(gpu_handle->id->id, 99);
  ASSERT_EQ(gpu_handle->offset, kOffset);
  ASSERT_EQ(gpu_handle->stride, kStride);

  handle = gpu_handle.To<gfx::GpuMemoryBufferHandle>();
  ASSERT_EQ(handle.type, gfx::SHARED_MEMORY_BUFFER);
  ASSERT_EQ(handle.id, kId);
  ASSERT_EQ(handle.offset, kOffset);
  ASSERT_EQ(handle.stride, kStride);

  base::SharedMemory shared_memory1(handle.handle, true);
  ASSERT_TRUE(shared_memory1.Map(1024));
}
