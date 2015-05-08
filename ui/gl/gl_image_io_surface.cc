// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_io_surface.h"

#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"

// Note that this must be included after gl_bindings.h to avoid conflicts.
#include <OpenGL/CGLIOSurface.h>

namespace gfx {
namespace {

bool ValidInternalFormat(unsigned internalformat) {
  switch (internalformat) {
    case GL_R8:
    case GL_BGRA_EXT:
      return true;
    default:
      return false;
  }
}

bool ValidFormat(GpuMemoryBuffer::Format format) {
  switch (format) {
    case GpuMemoryBuffer::R_8:
    case GpuMemoryBuffer::BGRA_8888:
      return true;
    case GpuMemoryBuffer::ATC:
    case GpuMemoryBuffer::ATCIA:
    case GpuMemoryBuffer::DXT1:
    case GpuMemoryBuffer::DXT5:
    case GpuMemoryBuffer::ETC1:
    case GpuMemoryBuffer::RGBA_8888:
    case GpuMemoryBuffer::RGBX_8888:
    case GpuMemoryBuffer::YUV_420:
      return false;
  }

  NOTREACHED();
  return false;
}

GLenum TextureFormat(GpuMemoryBuffer::Format format) {
  switch (format) {
    case GpuMemoryBuffer::R_8:
      return GL_RED;
    case GpuMemoryBuffer::BGRA_8888:
      return GL_RGBA;
    case GpuMemoryBuffer::ATC:
    case GpuMemoryBuffer::ATCIA:
    case GpuMemoryBuffer::DXT1:
    case GpuMemoryBuffer::DXT5:
    case GpuMemoryBuffer::ETC1:
    case GpuMemoryBuffer::RGBA_8888:
    case GpuMemoryBuffer::RGBX_8888:
    case GpuMemoryBuffer::YUV_420:
      NOTREACHED();
      return 0;
  }

  NOTREACHED();
  return 0;
}

GLenum DataFormat(GpuMemoryBuffer::Format format) {
  switch (format) {
    case GpuMemoryBuffer::R_8:
      return GL_RED;
    case GpuMemoryBuffer::BGRA_8888:
      return GL_BGRA;
    case GpuMemoryBuffer::ATC:
    case GpuMemoryBuffer::ATCIA:
    case GpuMemoryBuffer::DXT1:
    case GpuMemoryBuffer::DXT5:
    case GpuMemoryBuffer::ETC1:
    case GpuMemoryBuffer::RGBA_8888:
    case GpuMemoryBuffer::RGBX_8888:
    case GpuMemoryBuffer::YUV_420:
      NOTREACHED();
      return 0;
  }

  NOTREACHED();
  return 0;
}

GLenum DataType(GpuMemoryBuffer::Format format) {
  switch (format) {
    case GpuMemoryBuffer::R_8:
      return GL_UNSIGNED_BYTE;
    case GpuMemoryBuffer::BGRA_8888:
      return GL_UNSIGNED_INT_8_8_8_8_REV;
    case GpuMemoryBuffer::ATC:
    case GpuMemoryBuffer::ATCIA:
    case GpuMemoryBuffer::DXT1:
    case GpuMemoryBuffer::DXT5:
    case GpuMemoryBuffer::ETC1:
    case GpuMemoryBuffer::RGBA_8888:
    case GpuMemoryBuffer::RGBX_8888:
    case GpuMemoryBuffer::YUV_420:
      NOTREACHED();
      return 0;
  }

  NOTREACHED();
  return 0;
}

}  // namespace

GLImageIOSurface::GLImageIOSurface(const gfx::Size& size,
                                   unsigned internalformat)
    : size_(size),
      internalformat_(internalformat),
      format_(GpuMemoryBuffer::RGBA_8888) {
}

GLImageIOSurface::~GLImageIOSurface() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!io_surface_);
}

bool GLImageIOSurface::Initialize(IOSurfaceRef io_surface,
                                  GpuMemoryBuffer::Format format) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!io_surface_);

  if (!ValidInternalFormat(internalformat_)) {
    LOG(ERROR) << "Invalid internalformat: " << internalformat_;
    return false;
  }

  if (!ValidFormat(format)) {
    LOG(ERROR) << "Invalid format: " << format;
    return false;
  }

  format_ = format;
  io_surface_.reset(io_surface, base::scoped_policy::RETAIN);
  return true;
}

void GLImageIOSurface::Destroy(bool have_context) {
  DCHECK(thread_checker_.CalledOnValidThread());
  io_surface_.reset();
}

gfx::Size GLImageIOSurface::GetSize() { return size_; }

bool GLImageIOSurface::BindTexImage(unsigned target) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (target != GL_TEXTURE_RECTANGLE_ARB) {
    // This might be supported in the future. For now, perform strict
    // validation so we know what's going on.
    LOG(ERROR) << "IOSurface requires TEXTURE_RECTANGLE_ARB target";
    return false;
  }

  CGLContextObj cgl_context =
      static_cast<CGLContextObj>(GLContext::GetCurrent()->GetHandle());

  DCHECK(io_surface_);
  CGLError cgl_error =
      CGLTexImageIOSurface2D(cgl_context, target, TextureFormat(format_),
                             size_.width(), size_.height(), DataFormat(format_),
                             DataType(format_), io_surface_.get(), 0);
  if (cgl_error != kCGLNoError) {
    LOG(ERROR) << "Error in CGLTexImageIOSurface2D";
    return false;
  }

  return true;
}

bool GLImageIOSurface::CopyTexImage(unsigned target) {
  return false;
}

bool GLImageIOSurface::ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                                            int z_order,
                                            OverlayTransform transform,
                                            const Rect& bounds_rect,
                                            const RectF& crop_rect) {
  return false;
}

}  // namespace gfx
