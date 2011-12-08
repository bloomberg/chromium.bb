// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gl/gl_surface_osmesa.h"
#include "base/logging.h"
#include "ui/gfx/gl/gl_bindings.h"
#include "ui/gfx/gl/gl_context.h"

namespace gfx {

GLSurfaceOSMesa::GLSurfaceOSMesa(unsigned format, const gfx::Size& size)
    : format_(format),
      size_(size) {
}

GLSurfaceOSMesa::~GLSurfaceOSMesa() {
  Destroy();
}

bool GLSurfaceOSMesa::Resize(const gfx::Size& new_size) {
  if (new_size == size_)
    return true;

  GLContext* current_context = GLContext::GetCurrent();
  bool was_current = current_context && current_context->IsCurrent(this);
  if (was_current)
    current_context->ReleaseCurrent(this);

  // Preserve the old buffer.
  scoped_array<int32> old_buffer(buffer_.release());

  // Allocate a new one.
  AllocateBuffer(new_size);

  // Copy the old back buffer into the new buffer.
  int copy_width = std::min(size_.width(), new_size.width());
  int copy_height = std::min(size_.height(), new_size.height());
  for (int y = 0; y < copy_height; ++y) {
    for (int x = 0; x < copy_width; ++x) {
      buffer_[y * new_size.width() + x] = old_buffer[y * size_.width() + x];
    }
  }

  size_ = new_size;

  if (was_current)
    return current_context->MakeCurrent(this);

  return true;
}

bool GLSurfaceOSMesa::Initialize() {
  AllocateBuffer(size_);
  return true;
}

void GLSurfaceOSMesa::Destroy() {
  buffer_.reset();
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

unsigned GLSurfaceOSMesa::GetFormat() {
  return format_;
}

void GLSurfaceOSMesa::AllocateBuffer(const Size& size) {
  buffer_.reset(new int32[size.GetArea()]);
  memset(buffer_.get(), 0, size.GetArea() * sizeof(buffer_[0]));
}

}  // namespace gfx
