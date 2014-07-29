// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/gpu_memory_buffer_factory_ozone_native_buffer.h"

#include "base/logging.h"

namespace ui {

// static
GpuMemoryBufferFactoryOzoneNativeBuffer*
    GpuMemoryBufferFactoryOzoneNativeBuffer::impl_ = NULL;

GpuMemoryBufferFactoryOzoneNativeBuffer::
    GpuMemoryBufferFactoryOzoneNativeBuffer() {
  CHECK(!impl_) << "There should only be a single "
                   "GpuMemoryBufferFactoryOzoneNativeBuffer.";
  impl_ = this;
}

GpuMemoryBufferFactoryOzoneNativeBuffer::
    ~GpuMemoryBufferFactoryOzoneNativeBuffer() {
  CHECK_EQ(impl_, this);
  impl_ = NULL;
}

GpuMemoryBufferFactoryOzoneNativeBuffer*
GpuMemoryBufferFactoryOzoneNativeBuffer::GetInstance() {
  CHECK(impl_)
      << "No GpuMemoryBufferFactoryOzoneNativeBuffer implementation set.";
  return impl_;
}

bool GpuMemoryBufferFactoryOzoneNativeBuffer::CreateGpuMemoryBuffer(
    const gfx::GpuMemoryBufferId& id,
    const gfx::Size& size,
    unsigned internalformat,
    unsigned usage) {
  NOTIMPLEMENTED();
  return false;
}

void GpuMemoryBufferFactoryOzoneNativeBuffer::DestroyGpuMemoryBuffer(
    const gfx::GpuMemoryBufferId& id) {
  NOTIMPLEMENTED();
}

scoped_refptr<gfx::GLImage>
GpuMemoryBufferFactoryOzoneNativeBuffer::CreateImageForGpuMemoryBuffer(
    const gfx::GpuMemoryBufferId& id,
    const gfx::Size& size,
    unsigned internalformat) {
  NOTIMPLEMENTED();
  return scoped_refptr<gfx::GLImage>();
}

}  // namespace ui
