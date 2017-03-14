// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_dxgi.h"

#include "third_party/khronos/EGL/egl.h"
#include "third_party/khronos/EGL/eglext.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_surface_egl.h"

namespace gl {

GLImageDXGI::GLImageDXGI(const gfx::Size& size, EGLStreamKHR stream)
    : size_(size), stream_(stream) {}

// static
GLImageDXGI* GLImageDXGI::FromGLImage(GLImage* image) {
  if (!image || image->GetType() != Type::DXGI_IMAGE)
    return nullptr;
  return static_cast<GLImageDXGI*>(image);
}

gfx::Size GLImageDXGI::GetSize() {
  return size_;
}

unsigned GLImageDXGI::GetInternalFormat() {
  return GL_BGRA_EXT;
}

bool GLImageDXGI::BindTexImage(unsigned target) {
  return false;
}

void GLImageDXGI::ReleaseTexImage(unsigned target) {}

bool GLImageDXGI::CopyTexImage(unsigned target) {
  return false;
}

bool GLImageDXGI::CopyTexSubImage(unsigned target,
                                  const gfx::Point& offset,
                                  const gfx::Rect& rect) {
  return false;
}

bool GLImageDXGI::ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                                       int z_order,
                                       gfx::OverlayTransform transform,
                                       const gfx::Rect& bounds_rect,
                                       const gfx::RectF& crop_rect) {
  return false;
}

void GLImageDXGI::Flush() {}

void GLImageDXGI::OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                               uint64_t process_tracing_id,
                               const std::string& dump_name) {}

GLImage::Type GLImageDXGI::GetType() const {
  return Type::DXGI_IMAGE;
}

void GLImageDXGI::SetTexture(
    const base::win::ScopedComPtr<ID3D11Texture2D>& texture,
    size_t level) {
  texture_ = texture;
  level_ = level;
}

GLImageDXGI::~GLImageDXGI() {
  EGLDisplay egl_display = gl::GLSurfaceEGL::GetHardwareDisplay();
  eglDestroyStreamKHR(egl_display, stream_);
}

}  // namespace gl
