// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_d3d.h"

#include "ui/gl/egl_util.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_surface_egl.h"

#ifndef EGL_ANGLE_image_d3d11_texture
#define EGL_D3D11_TEXTURE_ANGLE 0x3484
#endif /* EGL_ANGLE_image_d3d11_texture */

namespace gl {

GLImageD3D::GLImageD3D(const gfx::Size& size,
                       gfx::BufferFormat buffer_format,
                       Microsoft::WRL::ComPtr<ID3D11Texture2D> texture,
                       Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain)
    : GLImage(),
      size_(size),
      buffer_format_(buffer_format),
      texture_(std::move(texture)),
      swap_chain_(std::move(swap_chain)) {
  DCHECK(texture_);
  DCHECK(swap_chain_);
}

GLImageD3D::~GLImageD3D() {
  if (egl_image_ != EGL_NO_IMAGE_KHR) {
    if (eglDestroyImageKHR(GLSurfaceEGL::GetHardwareDisplay(), egl_image_) ==
        EGL_FALSE) {
      DLOG(ERROR) << "Error destroying EGLImage: "
                  << ui::GetLastEGLErrorString();
    }
  }
}

bool GLImageD3D::Initialize() {
  DCHECK_EQ(egl_image_, EGL_NO_IMAGE_KHR);
  const EGLint attribs[] = {EGL_NONE};
  egl_image_ =
      eglCreateImageKHR(GLSurfaceEGL::GetHardwareDisplay(), EGL_NO_CONTEXT,
                        EGL_D3D11_TEXTURE_ANGLE,
                        static_cast<EGLClientBuffer>(texture_.Get()), attribs);
  if (egl_image_ == EGL_NO_IMAGE_KHR) {
    LOG(ERROR) << "Error creating EGLImage: " << ui::GetLastEGLErrorString();
    return false;
  }
  return true;
}

// static
GLImageD3D* GLImageD3D::FromGLImage(GLImage* image) {
  if (!image || image->GetType() != Type::D3D)
    return nullptr;
  return static_cast<GLImageD3D*>(image);
}

GLImage::Type GLImageD3D::GetType() const {
  return Type::D3D;
}

GLImage::BindOrCopy GLImageD3D::ShouldBindOrCopy() {
  return GLImage::BIND;
}

gfx::Size GLImageD3D::GetSize() {
  return size_;
}

unsigned GLImageD3D::GetInternalFormat() {
  return buffer_format_ == gfx::BufferFormat::RGBA_F16 ? GL_RGBA16F_EXT
                                                       : GL_BGRA8_EXT;
}

bool GLImageD3D::BindTexImage(unsigned target) {
  DCHECK_NE(egl_image_, EGL_NO_IMAGE_KHR);
  glEGLImageTargetTexture2DOES(target, egl_image_);
  return glGetError() == static_cast<GLenum>(GL_NO_ERROR);
}

bool GLImageD3D::CopyTexImage(unsigned target) {
  NOTREACHED();
  return false;
}

bool GLImageD3D::CopyTexSubImage(unsigned target,
                                 const gfx::Point& offset,
                                 const gfx::Rect& rect) {
  NOTREACHED();
  return false;
}

void GLImageD3D::OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                              uint64_t process_tracing_id,
                              const std::string& dump_name) {
  NOTIMPLEMENTED_LOG_ONCE();
}

bool GLImageD3D::ScheduleOverlayPlane(
    gfx::AcceleratedWidget widget,
    int z_order,
    gfx::OverlayTransform transform,
    const gfx::Rect& bounds_rect,
    const gfx::RectF& crop_rect,
    bool enable_blend,
    std::unique_ptr<gfx::GpuFence> gpu_fence) {
  NOTREACHED();
  return false;
}

}  // namespace gl
