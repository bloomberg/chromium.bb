// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

extern "C" {
#include <X11/Xlib.h>
}

#include "ui/gfx/gl/gl_context_glx.h"

#include "base/logging.h"
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

GLContextGLX::GLContextGLX(GLSurfaceGLX* surface)
  : surface_(surface),
    context_(NULL) {
}

GLContextGLX::~GLContextGLX() {
  Destroy();
}

bool GLContextGLX::Initialize(GLContext* shared_context) {
  context_ = glXCreateNewContext(
      GLSurfaceGLX::GetDisplay(),
      static_cast<GLXFBConfig>(surface_->GetConfig()),
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

bool GLContextGLX::MakeCurrent() {
  if (IsCurrent()) {
    return true;
  }

  if (!glXMakeContextCurrent(
      GLSurfaceGLX::GetDisplay(),
      reinterpret_cast<GLXDrawable>(surface_->GetHandle()),
      reinterpret_cast<GLXDrawable>(surface_->GetHandle()),
      static_cast<GLXContext>(context_))) {
    Destroy();
    LOG(ERROR) << "Couldn't make context current.";
    return false;
  }

  return true;
}

bool GLContextGLX::IsCurrent() {
  // TODO(apatrick): When surface is split from context, cannot use surface_
  // here.
  return glXGetCurrentDrawable() ==
      reinterpret_cast<GLXDrawable>(surface_->GetHandle()) &&
      glXGetCurrentContext() == static_cast<GLXContext>(context_);
}

bool GLContextGLX::IsOffscreen() {
  // TODO(apatrick): remove this from GLContext interface.
  return surface_->IsOffscreen();
}

bool GLContextGLX::SwapBuffers() {
  // TODO(apatrick): remove this from GLContext interface.
  return surface_->SwapBuffers();
}

gfx::Size GLContextGLX::GetSize() {
  // TODO(apatrick): remove this from GLContext interface.
  return surface_->GetSize();
}

void* GLContextGLX::GetHandle() {
  return context_;
}

void GLContextGLX::SetSwapInterval(int interval) {
  DCHECK(IsCurrent());
  if (HasExtension("GLX_EXT_swap_control") && glXSwapIntervalEXT) {
    // Only enable vsync if we aren't using a compositing window
    // manager. At the moment, compositing window managers don't
    // respect this setting anyway (tearing still occurs) and it
    // dramatically increases latency.
    if (!IsCompositingWindowManagerActive(GLSurfaceGLX::GetDisplay())) {
      glXSwapIntervalEXT(
          GLSurfaceGLX::GetDisplay(),
          reinterpret_cast<GLXDrawable>(surface_->GetHandle()),
          interval);
    }
  }
}

std::string GLContextGLX::GetExtensions() {
  DCHECK(IsCurrent());
  const char* extensions = glXQueryExtensionsString(
      GLSurfaceGLX::GetDisplay(),
      0);
  if (extensions) {
    return GLContext::GetExtensions() + " " + extensions;
  }

  return GLContext::GetExtensions();
}

}  // namespace gfx
