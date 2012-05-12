// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_context_cgl.h"

#include <OpenGL/CGLRenderers.h>
#include <vector>

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_cgl.h"

namespace gfx {

GLContextCGL::GLContextCGL(GLShareGroup* share_group)
  : GLContext(share_group),
    context_(NULL),
    gpu_preference_(PreferIntegratedGpu) {
}

bool GLContextCGL::Initialize(GLSurface* compatible_surface,
                              GpuPreference gpu_preference) {
  DCHECK(compatible_surface);

  GLContextCGL* share_context = share_group() ?
      static_cast<GLContextCGL*>(share_group()->GetContext()) : NULL;
  if (SupportsDualGpus()) {
    // Ensure the GPU preference is compatible with contexts already in the
    // share group.
    if (share_context && gpu_preference != share_context->GetGpuPreference())
      return false;
  }

  std::vector<CGLPixelFormatAttribute> attribs;
  bool using_offline_renderer =
      SupportsDualGpus() && gpu_preference == PreferIntegratedGpu;
  if (using_offline_renderer) {
    attribs.push_back(kCGLPFAAllowOfflineRenderers);
  }
  if (GetGLImplementation() == kGLImplementationAppleGL) {
    attribs.push_back(kCGLPFARendererID);
    attribs.push_back((CGLPixelFormatAttribute) kCGLRendererGenericFloatID);
  }
  attribs.push_back((CGLPixelFormatAttribute) 0);

  CGLPixelFormatObj format;
  GLint num_pixel_formats;
  if (CGLChoosePixelFormat(&attribs.front(),
                           &format,
                           &num_pixel_formats) != kCGLNoError) {
    LOG(ERROR) << "Error choosing pixel format.";
    return false;
  }
  if (!format) {
    LOG(ERROR) << "format == 0.";
    return false;
  }
  DCHECK_NE(num_pixel_formats, 0);

  CGLError res = CGLCreateContext(
      format,
      share_context ?
          static_cast<CGLContextObj>(share_context->GetHandle()) : NULL,
      reinterpret_cast<CGLContextObj*>(&context_));
  CGLReleasePixelFormat(format);
  if (res != kCGLNoError) {
    LOG(ERROR) << "Error creating context.";
    Destroy();
    return false;
  }

  gpu_preference_ = gpu_preference;
  return true;
}

void GLContextCGL::Destroy() {
  if (context_) {
    CGLDestroyContext(static_cast<CGLContextObj>(context_));
    context_ = NULL;
  }
}

bool GLContextCGL::MakeCurrent(GLSurface* surface) {
  DCHECK(context_);
  if (IsCurrent(surface))
    return true;

  TRACE_EVENT0("gpu", "GLContextCGL::MakeCurrent");

  if (CGLSetCurrentContext(
      static_cast<CGLContextObj>(context_)) != kCGLNoError) {
    LOG(ERROR) << "Unable to make gl context current.";
    return false;
  }

  SetCurrent(this, surface);
  if (!InitializeExtensionBindings()) {
    ReleaseCurrent(surface);
    return false;
  }

  if (!surface->OnMakeCurrent(this)) {
    LOG(ERROR) << "Unable to make gl context current.";
    return false;
  }

  return true;
}

void GLContextCGL::ReleaseCurrent(GLSurface* surface) {
  if (!IsCurrent(surface))
    return;

  SetCurrent(NULL, NULL);
  CGLSetCurrentContext(NULL);
}

bool GLContextCGL::IsCurrent(GLSurface* surface) {
  bool native_context_is_current = CGLGetCurrentContext() == context_;

  // If our context is current then our notion of which GLContext is
  // current must be correct. On the other hand, third-party code
  // using OpenGL might change the current context.
  DCHECK(!native_context_is_current || (GetCurrent() == this));

  if (!native_context_is_current)
    return false;

  return true;
}

void* GLContextCGL::GetHandle() {
  return context_;
}

void GLContextCGL::SetSwapInterval(int interval) {
  DCHECK(IsCurrent(NULL));
  LOG(WARNING) << "GLContex: GLContextCGL::SetSwapInterval is ignored.";
}

GLContextCGL::~GLContextCGL() {
  Destroy();
}

GpuPreference GLContextCGL::GetGpuPreference() {
  return gpu_preference_;
}

void GLContextCGL::ForceUseOfDiscreteGPU() {
  static CGLPixelFormatObj format = NULL;
  if (format)
    return;
  CGLPixelFormatAttribute attribs[1];
  attribs[0] = static_cast<CGLPixelFormatAttribute>(0);
  GLint num_pixel_formats = 0;
  CGLChoosePixelFormat(attribs, &format, &num_pixel_formats);
  // format is deliberately leaked.
}

}  // namespace gfx
