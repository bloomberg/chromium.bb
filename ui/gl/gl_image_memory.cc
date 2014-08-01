// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_memory.h"

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/scoped_binders.h"

#if defined(OS_WIN) || defined(USE_X11) || defined(OS_ANDROID) || \
    defined(USE_OZONE)
#include "ui/gl/gl_surface_egl.h"
#endif

namespace gfx {
namespace {

bool ValidFormat(unsigned internalformat) {
  switch (internalformat) {
    case GL_BGRA8_EXT:
    case GL_RGBA8_OES:
      return true;
    default:
      return false;
  }
}

GLenum TextureFormat(unsigned internalformat) {
  switch (internalformat) {
    case GL_BGRA8_EXT:
      return GL_BGRA_EXT;
    case GL_RGBA8_OES:
      return GL_RGBA;
    default:
      NOTREACHED();
      return 0;
  }
}

GLenum DataFormat(unsigned internalformat) {
  return TextureFormat(internalformat);
}

GLenum DataType(unsigned internalformat) {
  switch (internalformat) {
    case GL_BGRA8_EXT:
    case GL_RGBA8_OES:
      return GL_UNSIGNED_BYTE;
    default:
      NOTREACHED();
      return 0;
  }
}

int BytesPerPixel(unsigned internalformat) {
  switch (internalformat) {
    case GL_BGRA8_EXT:
    case GL_RGBA8_OES:
      return 4;
    default:
      NOTREACHED();
      return 0;
  }
}

}  // namespace

GLImageMemory::GLImageMemory(const gfx::Size& size, unsigned internalformat)
    : memory_(NULL),
      size_(size),
      internalformat_(internalformat)
#if defined(OS_WIN) || defined(USE_X11) || defined(OS_ANDROID) || \
    defined(USE_OZONE)
      ,
      egl_texture_id_(0u),
      egl_image_(EGL_NO_IMAGE_KHR)
#endif
{
}

GLImageMemory::~GLImageMemory() {
#if defined(OS_WIN) || defined(USE_X11) || defined(OS_ANDROID) || \
    defined(USE_OZONE)
  DCHECK_EQ(EGL_NO_IMAGE_KHR, egl_image_);
  DCHECK_EQ(0u, egl_texture_id_);
#endif
}

bool GLImageMemory::Initialize(const unsigned char* memory) {
  if (!ValidFormat(internalformat_)) {
    DVLOG(0) << "Invalid format: " << internalformat_;
    return false;
  }

  DCHECK(memory);
  DCHECK(!memory_);
  memory_ = memory;
  return true;
}

void GLImageMemory::Destroy(bool have_context) {
#if defined(OS_WIN) || defined(USE_X11) || defined(OS_ANDROID) || \
    defined(USE_OZONE)
  if (egl_image_ != EGL_NO_IMAGE_KHR) {
    eglDestroyImageKHR(GLSurfaceEGL::GetHardwareDisplay(), egl_image_);
    egl_image_ = EGL_NO_IMAGE_KHR;
  }

  if (egl_texture_id_) {
    if (have_context)
      glDeleteTextures(1, &egl_texture_id_);
    egl_texture_id_ = 0u;
  }
#endif
  memory_ = NULL;
}

gfx::Size GLImageMemory::GetSize() {
  return size_;
}

bool GLImageMemory::BindTexImage(unsigned target) {
  TRACE_EVENT0("gpu", "GLImageMemory::BindTexImage");

  DCHECK(memory_);
#if defined(OS_WIN) || defined(USE_X11) || defined(OS_ANDROID) || \
    defined(USE_OZONE)
  if (target == GL_TEXTURE_EXTERNAL_OES) {
    if (egl_image_ == EGL_NO_IMAGE_KHR) {
      DCHECK_EQ(0u, egl_texture_id_);
      glGenTextures(1, &egl_texture_id_);

      {
        ScopedTextureBinder texture_binder(GL_TEXTURE_2D, egl_texture_id_);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D,
                     0,  // mip level
                     TextureFormat(internalformat_),
                     size_.width(),
                     size_.height(),
                     0,  // border
                     DataFormat(internalformat_),
                     DataType(internalformat_),
                     memory_);
      }

      EGLint attrs[] = {EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE};
      // Need to pass current EGL rendering context to eglCreateImageKHR for
      // target type EGL_GL_TEXTURE_2D_KHR.
      egl_image_ =
          eglCreateImageKHR(GLSurfaceEGL::GetHardwareDisplay(),
                            eglGetCurrentContext(),
                            EGL_GL_TEXTURE_2D_KHR,
                            reinterpret_cast<EGLClientBuffer>(egl_texture_id_),
                            attrs);
      DCHECK_NE(EGL_NO_IMAGE_KHR, egl_image_)
          << "Error creating EGLImage: " << eglGetError();
    } else {
      ScopedTextureBinder texture_binder(GL_TEXTURE_2D, egl_texture_id_);

      glTexSubImage2D(GL_TEXTURE_2D,
                      0,  // mip level
                      0,  // x-offset
                      0,  // y-offset
                      size_.width(),
                      size_.height(),
                      DataFormat(internalformat_),
                      DataType(internalformat_),
                      memory_);
    }

    glEGLImageTargetTexture2DOES(target, egl_image_);
    DCHECK_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

    return true;
  }
#endif

  DCHECK_NE(static_cast<GLenum>(GL_TEXTURE_EXTERNAL_OES), target);
  glTexImage2D(target,
               0,  // mip level
               TextureFormat(internalformat_),
               size_.width(),
               size_.height(),
               0,  // border
               DataFormat(internalformat_),
               DataType(internalformat_),
               memory_);

  return true;
}

bool GLImageMemory::HasValidFormat() const {
  return ValidFormat(internalformat_);
}

size_t GLImageMemory::Bytes() const {
  return size_.GetArea() * BytesPerPixel(internalformat_);
}

bool GLImageMemory::ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                                         int z_order,
                                         OverlayTransform transform,
                                         const Rect& bounds_rect,
                                         const RectF& crop_rect) {
  return false;
}

}  // namespace gfx
