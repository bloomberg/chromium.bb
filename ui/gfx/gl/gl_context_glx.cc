// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

extern "C" {
#include <X11/Xlib.h>
}

#include "ui/gfx/gl/gl_context_glx.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/mesa/MesaLib/include/GL/osmesa.h"
#include "ui/gfx/gl/gl_bindings.h"
#include "ui/gfx/gl/gl_implementation.h"
#include "ui/gfx/gl/gl_surface_glx.h"

namespace gfx {

namespace {

bool IsCompositingWindowManagerActive(Display* display) {
  // The X macro "None" has been undefined by gl_bindings.h.
  const int kNone = 0;
  static Atom net_wm_cm_s0 = kNone;
  if (net_wm_cm_s0 == kNone) {
    net_wm_cm_s0 = XInternAtom(display, "_NET_WM_CM_S0", True);
  }
  if (net_wm_cm_s0 == kNone) {
    return false;
  }
  return XGetSelectionOwner(display, net_wm_cm_s0) != kNone;
}

}  // namespace anonymous

GLContextGLX::GLContextGLX()
  : context_(NULL) {
}

GLContextGLX::~GLContextGLX() {
  Destroy();
}

bool GLContextGLX::Initialize(GLContext* shared_context,
                              GLSurface* compatible_surface) {
  GLSurfaceGLX* surface_glx = static_cast<GLSurfaceGLX*>(compatible_surface);
  context_ = glXCreateNewContext(
      GLSurfaceGLX::GetDisplay(),
      static_cast<GLXFBConfig>(surface_glx->GetConfig()),
      GLX_RGBA_TYPE,
      static_cast<GLXContext>(
          shared_context ? shared_context->GetHandle() : NULL),
      True);
  if (!context_) {
    LOG(ERROR) << "Couldn't create GL context.";
    Destroy();
    return false;
  }

  return true;
}

void GLContextGLX::Destroy() {
  if (context_) {
    glXDestroyContext(GLSurfaceGLX::GetDisplay(),
                      static_cast<GLXContext>(context_));
    context_ = NULL;
  }
}

bool GLContextGLX::MakeCurrent(GLSurface* surface) {
  DCHECK(context_);
  if (IsCurrent(surface))
    return true;

  if (!glXMakeContextCurrent(
      GLSurfaceGLX::GetDisplay(),
      reinterpret_cast<GLXDrawable>(surface->GetHandle()),
      reinterpret_cast<GLXDrawable>(surface->GetHandle()),
      static_cast<GLXContext>(context_))) {
    Destroy();
    LOG(ERROR) << "Couldn't make context current.";
    return false;
  }

  return true;
}

void GLContextGLX::ReleaseCurrent(GLSurface* surface) {
  if (!IsCurrent(surface))
    return;

  glXMakeContextCurrent(GLSurfaceGLX::GetDisplay(), 0, 0, NULL);
}

bool GLContextGLX::IsCurrent(GLSurface* surface) {
  if (glXGetCurrentContext() != static_cast<GLXContext>(context_))
    return false;

  if (surface) {
    if (glXGetCurrentDrawable() !=
        reinterpret_cast<GLXDrawable>(surface->GetHandle())) {
      return false;
    }
  }

  return true;
}

void* GLContextGLX::GetHandle() {
  return context_;
}

void GLContextGLX::SetSwapInterval(int interval) {
  DCHECK(IsCurrent(NULL));
  if (HasExtension("GLX_EXT_swap_control") && glXSwapIntervalEXT) {
    // Only enable vsync if we aren't using a compositing window
    // manager. At the moment, compositing window managers don't
    // respect this setting anyway (tearing still occurs) and it
    // dramatically increases latency.
    if (!IsCompositingWindowManagerActive(GLSurfaceGLX::GetDisplay())) {
      glXSwapIntervalEXT(
          GLSurfaceGLX::GetDisplay(),
          glXGetCurrentDrawable(),
          interval);
    }
  }
}

std::string GLContextGLX::GetExtensions() {
  DCHECK(IsCurrent(NULL));
  const char* extensions = glXQueryExtensionsString(
      GLSurfaceGLX::GetDisplay(),
      0);
  if (extensions) {
    return GLContext::GetExtensions() + " " + extensions;
  }

  return GLContext::GetExtensions();
}

}  // namespace gfx
