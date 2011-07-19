// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gl/gl_surface_cgl.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "ui/gfx/gl/gl_bindings.h"

namespace gfx {

namespace {
CGLPixelFormatObj g_pixel_format;
}

GLSurfaceCGL::GLSurfaceCGL() {
}

GLSurfaceCGL::~GLSurfaceCGL() {
}

bool GLSurfaceCGL::InitializeOneOff() {
  static bool initialized = false;
  if (initialized)
    return true;

  static const CGLPixelFormatAttribute attribs[] = {
    (CGLPixelFormatAttribute) kCGLPFAPBuffer,
    (CGLPixelFormatAttribute) 0
  };
  GLint num_pixel_formats;
  if (CGLChoosePixelFormat(attribs,
                           &g_pixel_format,
                           &num_pixel_formats) != kCGLNoError) {
    LOG(ERROR) << "Error choosing pixel format.";
    return false;
  }
  if (num_pixel_formats == 0) {
    LOG(ERROR) << "num_pixel_formats == 0.";
    return false;
  }
  if (!g_pixel_format) {
    LOG(ERROR) << "pixel_format == 0.";
    return false;
  }

  initialized = true;
  return true;
}

void* GLSurfaceCGL::GetPixelFormat() {
  return g_pixel_format;
}

PbufferGLSurfaceCGL::PbufferGLSurfaceCGL(const gfx::Size& size)
  : size_(size),
    pbuffer_(NULL) {
}

PbufferGLSurfaceCGL::~PbufferGLSurfaceCGL() {
  Destroy();
}

bool PbufferGLSurfaceCGL::Initialize() {
  if (CGLCreatePBuffer(size_.width(),
                       size_.height(),
                       GL_TEXTURE_2D,
                       GL_RGBA,
                       0,
                       reinterpret_cast<CGLPBufferObj*>(&pbuffer_))
      != kCGLNoError) {
    LOG(ERROR) << "Error creating pbuffer.";
    Destroy();
    return false;
  }

  return true;
}

void PbufferGLSurfaceCGL::Destroy() {
  if (pbuffer_) {
    CGLDestroyPBuffer(static_cast<CGLPBufferObj>(pbuffer_));
    pbuffer_ = NULL;
  }
}

bool PbufferGLSurfaceCGL::IsOffscreen() {
  return true;
}

bool PbufferGLSurfaceCGL::SwapBuffers() {
  NOTREACHED() << "Cannot call SwapBuffers on a PbufferGLSurfaceCGL.";
  return false;
}

gfx::Size PbufferGLSurfaceCGL::GetSize() {
  return size_;
}

void* PbufferGLSurfaceCGL::GetHandle() {
  return pbuffer_;
}

}  // namespace gfx
