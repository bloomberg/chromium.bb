// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_egl.h"

#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_surface_egl.h"

namespace gfx {

GLImageEGL::GLImageEGL(gfx::Size size)
    : egl_image_(EGL_NO_IMAGE_KHR),
      size_(size) {
}

GLImageEGL::~GLImageEGL() {
  Destroy();
}

bool GLImageEGL::Initialize(gfx::GpuMemoryBufferHandle buffer) {
  DCHECK(buffer.native_buffer);
  EGLint attrs[] = {
    EGL_IMAGE_PRESERVED_KHR, EGL_TRUE,
    EGL_NONE,
  };
  egl_image_ = eglCreateImageKHR(
      GLSurfaceEGL::GetHardwareDisplay(),
      EGL_NO_CONTEXT,
      EGL_NATIVE_BUFFER_ANDROID,
      buffer.native_buffer,
      attrs);

  if (egl_image_ == EGL_NO_IMAGE_KHR) {
    EGLint error = eglGetError();
    LOG(ERROR) << "Error creating EGLImage: " << error;
    return false;
  }

  return true;
}

bool GLImageEGL::BindTexImage() {
  if (egl_image_ == EGL_NO_IMAGE_KHR) {
    LOG(ERROR) << "NULL EGLImage in BindTexImage";
    return false;
  }

  glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, egl_image_);

  if (glGetError() != GL_NO_ERROR) {
    return false;
  }

  return true;
}

gfx::Size GLImageEGL::GetSize() {
  return size_;
}

void GLImageEGL::Destroy() {
  if (egl_image_ == EGL_NO_IMAGE_KHR)
    return;

  EGLBoolean success = eglDestroyImageKHR(
      GLSurfaceEGL::GetHardwareDisplay(), egl_image_);

  if (success == EGL_FALSE) {
    EGLint error = eglGetError();
    LOG(ERROR) << "Error destroying EGLImage: " << error;
  }

  egl_image_ = EGL_NO_IMAGE_KHR;
}

void GLImageEGL::ReleaseTexImage() {
  char zero[4] = { 0, };
  glTexImage2D(GL_TEXTURE_2D,
               0,
               GL_RGBA,
               1,
               1,
               0,
               GL_RGBA,
               GL_UNSIGNED_BYTE,
               &zero);
}

}  // namespace gfx
