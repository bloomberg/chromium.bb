// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/gpu/gpu_memory_buffer_factory_ozone_native_buffer.h"

#include "base/logging.h"
#include "ui/gl/gl_image.h"

namespace ui {

GpuMemoryBufferFactoryOzoneNativeBuffer::
    GpuMemoryBufferFactoryOzoneNativeBuffer() {
}

GpuMemoryBufferFactoryOzoneNativeBuffer::
    ~GpuMemoryBufferFactoryOzoneNativeBuffer() {
}

GpuMemoryBufferFactoryOzoneNativeBuffer*
GpuMemoryBufferFactoryOzoneNativeBuffer::GetInstance() {
  NOTREACHED();
  return NULL;
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
