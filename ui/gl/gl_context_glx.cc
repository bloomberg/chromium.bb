// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

extern "C" {
#include <X11/Xlib.h>
}

#include "ui/gl/gl_context_glx.h"

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/mesa/MesaLib/include/GL/osmesa.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_glx.h"

namespace gfx {

namespace {

// scoped_ptr functor for XFree(). Use as follows:
//   scoped_ptr_malloc<XVisualInfo, ScopedPtrXFree> foo(...);
// where "XVisualInfo" is any X type that is freed with XFree.
class ScopedPtrXFree {
 public:
  void operator()(void* x) const {
    ::XFree(x);
  }
};

}  // namespace anonymous

GLContextGLX::GLContextGLX(GLShareGroup* share_group)
  : GLContext(share_group),
    context_(NULL),
    display_(NULL) {
}

Display* GLContextGLX::display() {
  return display_;
}

bool GLContextGLX::Initialize(
    GLSurface* compatible_surface, GpuPreference gpu_preference) {
  display_ = static_cast<Display*>(compatible_surface->GetDisplay());

  GLXContext share_handle = static_cast<GLXContext>(
      share_group() ? share_group()->GetHandle() : NULL);

  if (GLSurfaceGLX::IsCreateContextRobustnessSupported()) {
    DLOG(INFO) << "GLX_ARB_create_context_robustness supported.";

    std::vector<int> attribs;
    attribs.push_back(GLX_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB);
    attribs.push_back(GLX_LOSE_CONTEXT_ON_RESET_ARB);
    attribs.push_back(0);
    context_ = glXCreateContextAttribsARB(
        display_,
        static_cast<GLXFBConfig>(compatible_surface->GetConfig()),
        share_handle,
        True,
        &attribs.front());
    if (context_) {
      DLOG(INFO) << "  Successfully allocated "
                 << (compatible_surface->IsOffscreen() ?
                     "offscreen" : "onscreen")
                 << " GL context with LOSE_CONTEXT_ON_RESET_ARB";
    } else {
      // TODO(kbr): it is not expected that things will work properly
      // in this case, since we will likely allocate our offscreen
      // contexts with this bit set and the onscreen contexts without,
      // and won't be able to put them in the same share group.
      // Consider what to do here; force loss of all contexts and
      // reallocation without ARB_robustness?
      LOG(ERROR) <<
          "  FAILED to allocate GL context with LOSE_CONTEXT_ON_RESET_ARB";
    }
  }

  if (!context_) {
    // The means by which the context is created depends on whether
    // the drawable type works reliably with GLX 1.3. If it does not
    // then fall back to GLX 1.2.
    if (compatible_surface->IsOffscreen()) {
      context_ = glXCreateNewContext(
          display_,
          static_cast<GLXFBConfig>(compatible_surface->GetConfig()),
          GLX_RGBA_TYPE,
          share_handle,
          True);
    } else {
      // Get the visuals for the X drawable.
      XWindowAttributes attributes;
      if (!XGetWindowAttributes(
          display_,
          reinterpret_cast<GLXDrawable>(compatible_surface->GetHandle()),
          &attributes)) {
        LOG(ERROR) << "XGetWindowAttributes failed for window " <<
            reinterpret_cast<GLXDrawable>(
                compatible_surface->GetHandle()) << ".";
        return false;
      }

      XVisualInfo visual_info_template;
      visual_info_template.visualid = XVisualIDFromVisual(attributes.visual);

      int visual_info_count = 0;
      scoped_ptr_malloc<XVisualInfo, ScopedPtrXFree> visual_info_list(
          XGetVisualInfo(display_, VisualIDMask,
                         &visual_info_template,
                         &visual_info_count));

      DCHECK(visual_info_list.get());
      if (visual_info_count == 0) {
        LOG(ERROR) << "No visual info for visual ID.";
        return false;
      }

      // Attempt to create a context with each visual in turn until one works.
      context_ = glXCreateContext(
          display_,
          visual_info_list.get(),
          share_handle,
          True);
    }
  }

  if (!context_) {
    LOG(ERROR) << "Couldn't create GL context.";
    return false;
  }

  DLOG(INFO) << (compatible_surface->IsOffscreen() ? "Offscreen" : "Onscreen")
             << " context was "
             << (glXIsDirect(display_,
                             static_cast<GLXContext>(context_))
                     ? "direct" : "indirect")
             << ".";

  return true;
}

void GLContextGLX::Destroy() {
  if (context_) {
    glXDestroyContext(display_,
                      static_cast<GLXContext>(context_));
    context_ = NULL;
  }
}

bool GLContextGLX::MakeCurrent(GLSurface* surface) {
  DCHECK(context_);
  if (IsCurrent(surface))
    return true;

  TRACE_EVENT0("gpu", "GLContextGLX::MakeCurrent");
  if (!glXMakeCurrent(
      display_,
      reinterpret_cast<GLXDrawable>(surface->GetHandle()),
      static_cast<GLXContext>(context_))) {
    LOG(ERROR) << "Couldn't make context current with X drawable.";
    Destroy();
    return false;
  }

  SetCurrent(this, surface);
  if (!InitializeExtensionBindings()) {
    ReleaseCurrent(surface);
    Destroy();
    return false;
  }

  if (!surface->OnMakeCurrent(this)) {
    LOG(ERROR) << "Could not make current.";
    ReleaseCurrent(surface);
    Destroy();
    return false;
  }

  return true;
}

void GLContextGLX::ReleaseCurrent(GLSurface* surface) {
  if (!IsCurrent(surface))
    return;

  SetCurrent(NULL, NULL);
  if (!glXMakeCurrent(display_, 0, 0))
    LOG(ERROR) << "glXMakeCurrent failed in ReleaseCurrent";
}

bool GLContextGLX::IsCurrent(GLSurface* surface) {
  bool native_context_is_current =
      glXGetCurrentContext() == static_cast<GLXContext>(context_);

  // If our context is current then our notion of which GLContext is
  // current must be correct. On the other hand, third-party code
  // using OpenGL might change the current context.
  DCHECK(!native_context_is_current || (GetCurrent() == this));

  if (!native_context_is_current)
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
    glXSwapIntervalEXT(
        display_,
        glXGetCurrentDrawable(),
        interval);
  } else {
    if(interval == 0)
      LOG(WARNING) <<
          "Could not disable vsync: driver does not "
          "support GLX_EXT_swap_control";
  }
}

std::string GLContextGLX::GetExtensions() {
  DCHECK(IsCurrent(NULL));
  const char* extensions = GLSurfaceGLX::GetGLXExtensions();
  if (extensions) {
    return GLContext::GetExtensions() + " " + extensions;
  }

  return GLContext::GetExtensions();
}

bool GLContextGLX::WasAllocatedUsingARBRobustness() {
  return GLSurfaceGLX::IsCreateContextRobustnessSupported();
}

GLContextGLX::~GLContextGLX() {
  Destroy();
}

}  // namespace gfx
