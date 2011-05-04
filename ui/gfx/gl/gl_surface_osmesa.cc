// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gl/gl_surface_osmesa.h"
#include "base/logging.h"
#include "ui/gfx/gl/gl_bindings.h"

namespace gfx {

GLSurfaceOSMesa::GLSurfaceOSMesa()
{
}

GLSurfaceOSMesa::~GLSurfaceOSMesa() {
}

void GLSurfaceOSMesa::Resize(const gfx::Size& new_size) {
  if (new_size == size_)
    return;

  // Allocate a new back buffer.
  scoped_array<int32> new_buffer(new int32[new_size.GetArea()]);
  memset(new_buffer.get(), 0, new_size.GetArea() * sizeof(new_buffer[0]));

  // Copy the current back buffer into the new buffer.
  int copy_width = std::min(size_.width(), new_size.width());
  int copy_height = std::min(size_.height(), new_size.height());
  for (int y = 0; y < copy_height; ++y) {
    for (int x = 0; x < copy_width; ++x) {
      new_buffer[y * new_size.width() + x] = buffer_[y * size_.width() + x];
    }
  }

  buffer_.reset(new_buffer.release());
  size_ = new_size;
}

bool GLSurfaceOSMesa::Initialize() {
  return true;
}

void GLSurfaceOSMesa::Destroy() {
  buffer_.reset();
  size_ = gfx::Size();
}

bool GLSurfaceOSMesa::IsOffscreen() {
  return true;
}

bool GLSurfaceOSMesa::SwapBuffers() {
  NOTREACHED() << "Should not call SwapBuffers on an GLSurfaceOSMesa.";
  return false;
}

gfx::Size GLSurfaceOSMesa::GetSize() {
  return size_;
}

void* GLSurfaceOSMesa::GetHandle() {
  return buffer_.get();
}

}  // namespace gfx
