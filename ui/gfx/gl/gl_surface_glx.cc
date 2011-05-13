// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

extern "C" {
#include <X11/Xlib.h>
}

#include "ui/gfx/gl/gl_surface_glx.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/mesa/MesaLib/include/GL/osmesa.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/gl/gl_bindings.h"
#include "ui/gfx/gl/gl_implementation.h"

namespace {

Display* g_display;

}  // namespace

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

GLSurfaceGLX::GLSurfaceGLX() {
}

GLSurfaceGLX::~GLSurfaceGLX() {
}

bool GLSurfaceGLX::InitializeOneOff() {
  static bool initialized = false;
  if (initialized)
    return true;

  g_display = XOpenDisplay(NULL);
  if (!g_display) {
    LOG(ERROR) << "XOpenDisplay failed.";
    return false;
  }

  int major, minor;
  if (!glXQueryVersion(g_display, &major, &minor)) {
    LOG(ERROR) << "glxQueryVersion failed";
    return false;
  }

  if (major == 1 && minor < 3) {
    LOG(ERROR) << "GLX 1.3 or later is required.";
    return false;
  }

  initialized = true;
  return true;
}

Display* GLSurfaceGLX::GetDisplay() {
  return g_display;
}

NativeViewGLSurfaceGLX::NativeViewGLSurfaceGLX(gfx::PluginWindowHandle window)
  : window_(window),
    config_(NULL),
    glx_window_(0) {
}

NativeViewGLSurfaceGLX::~NativeViewGLSurfaceGLX() {
  Destroy();
}

bool NativeViewGLSurfaceGLX::Initialize() {
  DCHECK(!glx_window_);

  XWindowAttributes attributes;
  XGetWindowAttributes(g_display, window_, &attributes);

  XVisualInfo visual_template = {0};
  visual_template.visualid = XVisualIDFromVisual(attributes.visual);

  int num_visual_infos;
  scoped_ptr_malloc<XVisualInfo, ScopedPtrXFree> visual_infos(
      XGetVisualInfo(g_display,
                     VisualIDMask,
                     &visual_template,
                     &num_visual_infos));

  if (!num_visual_infos)
    return false;

  if (glXGetFBConfigFromVisualSGIX) {
    config_ = glXGetFBConfigFromVisualSGIX(g_display, visual_infos.get());
    if (!config_) {
      LOG(ERROR) << "glXGetFBConfigFromVisualSGIX failed.";
    }
  }

  if (!config_) {
    int config_id;
    if (glXGetConfig(g_display,
                     visual_infos.get(),
                     GLX_FBCONFIG_ID,
                     &config_id)) {
      LOG(ERROR) << "glXGetConfig failed.";
      return false;
    }

    const int config_attributes[] = {
      GLX_FBCONFIG_ID, config_id,
      0
    };

    int num_elements = 0;
    scoped_ptr_malloc<GLXFBConfig, ScopedPtrXFree> configs(
        glXChooseFBConfig(g_display,
                          DefaultScreen(g_display),
                          config_attributes,
                          &num_elements));
    if (!configs.get()) {
      LOG(ERROR) << "glXChooseFBConfig failed.";
      return false;
    }
    if (!num_elements) {
      LOG(ERROR) << "glXChooseFBConfig returned 0 elements.";
      return false;
    }

    config_ = configs.get()[0];
  }

  glx_window_ = glXCreateWindow(g_display,
                                static_cast<GLXFBConfig>(config_),
                                window_,
                                NULL);
  if (!glx_window_) {
    Destroy();
    LOG(ERROR) << "glXCreateWindow failed.";
    return false;
  }

  return true;
}

void NativeViewGLSurfaceGLX::Destroy() {
  if (glx_window_) {
    glXDestroyWindow(g_display, glx_window_);
    glx_window_ = 0;
  }

  config_ = NULL;
}

bool NativeViewGLSurfaceGLX::IsOffscreen() {
  return false;
}

bool NativeViewGLSurfaceGLX::SwapBuffers() {
  glXSwapBuffers(g_display, glx_window_);
  return true;
}

gfx::Size NativeViewGLSurfaceGLX::GetSize() {
  XWindowAttributes attributes;
  XGetWindowAttributes(g_display, window_, &attributes);
  return gfx::Size(attributes.width, attributes.height);
}

void* NativeViewGLSurfaceGLX::GetHandle() {
  return reinterpret_cast<void*>(glx_window_);
}

void* NativeViewGLSurfaceGLX::GetConfig() {
  return config_;
}

PbufferGLSurfaceGLX::PbufferGLSurfaceGLX(const gfx::Size& size)
  : size_(size),
    pbuffer_(0),
    config_(NULL) {
}

PbufferGLSurfaceGLX::~PbufferGLSurfaceGLX() {
  Destroy();
}

bool PbufferGLSurfaceGLX::Initialize() {
  DCHECK(!pbuffer_);

  static const int config_attributes[] = {
    GLX_BUFFER_SIZE, 32,
    GLX_ALPHA_SIZE, 8,
    GLX_BLUE_SIZE, 8,
    GLX_RED_SIZE, 8,
    GLX_DEPTH_SIZE, 16,  // TODO(apatrick): support optional depth buffer
    GLX_STENCIL_SIZE, 8,
    GLX_RENDER_TYPE, GLX_RGBA_BIT,
    GLX_DRAWABLE_TYPE, GLX_PBUFFER_BIT,
    GLX_DOUBLEBUFFER, False,
    0
  };

  int num_elements = 0;
  scoped_ptr_malloc<GLXFBConfig, ScopedPtrXFree> configs(
      glXChooseFBConfig(g_display,
                        DefaultScreen(g_display),
                        config_attributes,
                        &num_elements));
  if (!configs.get()) {
    LOG(ERROR) << "glXChooseFBConfig failed.";
    return false;
  }
  if (!num_elements) {
    LOG(ERROR) << "glXChooseFBConfig returned 0 elements.";
    return false;
  }

  config_ = configs.get()[0];

  const int pbuffer_attributes[] = {
    GLX_PBUFFER_WIDTH, size_.width(),
    GLX_PBUFFER_HEIGHT, size_.height(),
    0
  };
  pbuffer_ = glXCreatePbuffer(g_display,
                              static_cast<GLXFBConfig>(config_),
                              pbuffer_attributes);
  if (!pbuffer_) {
    Destroy();
    LOG(ERROR) << "glXCreatePbuffer failed.";
    return false;
  }

  return true;
}

void PbufferGLSurfaceGLX::Destroy() {
  if (pbuffer_) {
    glXDestroyPbuffer(g_display, pbuffer_);
    pbuffer_ = 0;
  }

  config_ = NULL;
}

bool PbufferGLSurfaceGLX::IsOffscreen() {
  return true;
}

bool PbufferGLSurfaceGLX::SwapBuffers() {
  NOTREACHED() << "Attempted to call SwapBuffers on a pbuffer.";
  return false;
}

gfx::Size PbufferGLSurfaceGLX::GetSize() {
  return size_;
}

void* PbufferGLSurfaceGLX::GetHandle() {
  return reinterpret_cast<void*>(pbuffer_);
}

void* PbufferGLSurfaceGLX::GetConfig() {
  return config_;
}

}  // namespace gfx
