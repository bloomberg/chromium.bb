// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_io_surface.h"

#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/io_surface_support_mac.h"

namespace gfx {

GLImageIOSurface::GLImageIOSurface(gfx::Size size)
    : io_surface_support_(IOSurfaceSupport::Initialize()),
      size_(size) {
  CHECK(io_surface_support_);
}

GLImageIOSurface::~GLImageIOSurface() {
  Destroy();
}

bool GLImageIOSurface::Initialize(gfx::GpuMemoryBufferHandle buffer) {
  io_surface_.reset(io_surface_support_->IOSurfaceLookup(buffer.io_surface_id));
  if (!io_surface_) {
    LOG(ERROR) << "IOSurface lookup failed";
    return false;
  }

  return true;
}

void GLImageIOSurface::Destroy() {
}

gfx::Size GLImageIOSurface::GetSize() {
  return size_;
}

bool GLImageIOSurface::BindTexImage(unsigned target) {
  if (target != GL_TEXTURE_RECTANGLE_ARB) {
    // This might be supported in the future. For now, perform strict
    // validation so we know what's going on.
    LOG(ERROR) << "IOSurface requires TEXTURE_RECTANGLE_ARB target";
    return false;
  }

  CGLContextObj cgl_context = static_cast<CGLContextObj>(
      GLContext::GetCurrent()->GetHandle());

  DCHECK(io_surface_);
  CGLError cgl_error = io_surface_support_->CGLTexImageIOSurface2D(
      cgl_context,
      target,
      GL_RGBA,
      size_.width(),
      size_.height(),
      GL_BGRA,
      GL_UNSIGNED_INT_8_8_8_8_REV,
      io_surface_.get(),
      0);
  if (cgl_error != kCGLNoError) {
    LOG(ERROR) << "Error in CGLTexImageIOSurface2D";
    return false;
  }

  return true;
}

void GLImageIOSurface::ReleaseTexImage(unsigned target) {
}

void GLImageIOSurface::WillUseTexImage() {
}

void GLImageIOSurface::DidUseTexImage() {
}

}  // namespace gfx
