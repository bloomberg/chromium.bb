// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GL/osmesa.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/gl/gl_bindings.h"
#include "ui/gfx/gl/gl_context.h"
#include "ui/gfx/gl/gl_context_egl.h"
#include "ui/gfx/gl/gl_context_glx.h"
#include "ui/gfx/gl/gl_context_osmesa.h"
#include "ui/gfx/gl/gl_context_stub.h"
#include "ui/gfx/gl/gl_implementation.h"
#include "ui/gfx/gl/gl_surface_egl.h"
#include "ui/gfx/gl/gl_surface_glx.h"
#include "ui/gfx/gl/gl_surface_osmesa.h"

namespace gfx {

namespace {
Display* g_osmesa_display;
}  // namespace anonymous

// This OSMesa GL surface can use XLib to swap the contents of the buffer to a
// view.
class NativeViewGLSurfaceOSMesa : public GLSurfaceOSMesa {
 public:
  explicit NativeViewGLSurfaceOSMesa(gfx::PluginWindowHandle window);
  virtual ~NativeViewGLSurfaceOSMesa();

  static bool InitializeOneOff();

  // Initializes the GL context.
  bool Initialize();

  // Implement a subset of GLSurface.
  virtual void Destroy();
  virtual bool IsOffscreen();
  virtual bool SwapBuffers();

 private:
  bool UpdateSize();

  GC window_graphics_context_;
  gfx::PluginWindowHandle window_;
  GC pixmap_graphics_context_;
  Pixmap pixmap_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewGLSurfaceOSMesa);
};

bool GLContext::InitializeOneOff() {
  static bool initialized = false;
  if (initialized)
    return true;

  static const GLImplementation kAllowedGLImplementations[] = {
    kGLImplementationDesktopGL,
    kGLImplementationEGLGLES2,
    kGLImplementationOSMesaGL
  };

  if (!InitializeRequestedGLBindings(
      kAllowedGLImplementations,
      kAllowedGLImplementations + arraysize(kAllowedGLImplementations),
      kGLImplementationDesktopGL)) {
    LOG(ERROR) << "InitializeRequestedGLBindings failed.";
    return false;
  }

  switch (GetGLImplementation()) {
    case kGLImplementationDesktopGL:
      if (!GLSurfaceGLX::InitializeOneOff()) {
        LOG(ERROR) << "GLSurfaceGLX::InitializeOneOff failed.";
        return false;
      }
      break;
    case kGLImplementationEGLGLES2:
      if (!GLSurfaceEGL::InitializeOneOff()) {
        LOG(ERROR) << "GLSurfaceEGL::InitializeOneOff failed.";
        return false;
      }
      break;
    case kGLImplementationOSMesaGL:
      if (!NativeViewGLSurfaceOSMesa::InitializeOneOff()) {
        LOG(ERROR) << "NativeViewGLSurfaceOSMesa::InitializeOneOff failed.";
        return false;
      }
      break;
    default:
      break;
  }

  initialized = true;
  return true;
}

NativeViewGLSurfaceOSMesa::NativeViewGLSurfaceOSMesa(
    gfx::PluginWindowHandle window)
  : window_graphics_context_(0),
    window_(window),
    pixmap_graphics_context_(0),
    pixmap_(0) {
  DCHECK(window);
}

NativeViewGLSurfaceOSMesa::~NativeViewGLSurfaceOSMesa() {
  Destroy();
}

bool NativeViewGLSurfaceOSMesa::InitializeOneOff() {
  static bool initialized = false;
  if (initialized)
    return true;

  g_osmesa_display = XOpenDisplay(NULL);
  if (!g_osmesa_display) {
    LOG(ERROR) << "XOpenDisplay failed.";
    return false;
  }

  initialized = true;
  return true;
}

bool NativeViewGLSurfaceOSMesa::Initialize() {
  window_graphics_context_ = XCreateGC(g_osmesa_display,
                                       window_,
                                       0,
                                       NULL);
  if (!window_graphics_context_) {
    LOG(ERROR) << "XCreateGC failed.";
    Destroy();
    return false;
  }

  UpdateSize();

  return true;
}

void NativeViewGLSurfaceOSMesa::Destroy() {
  if (pixmap_graphics_context_) {
    XFreeGC(g_osmesa_display, pixmap_graphics_context_);
    pixmap_graphics_context_ = NULL;
  }

  if (pixmap_) {
    XFreePixmap(g_osmesa_display, pixmap_);
    pixmap_ = 0;
  }

  if (window_graphics_context_) {
    XFreeGC(g_osmesa_display, window_graphics_context_);
    window_graphics_context_ = NULL;
  }
}

bool NativeViewGLSurfaceOSMesa::IsOffscreen() {
  return false;
}

bool NativeViewGLSurfaceOSMesa::SwapBuffers() {
  // Update the size before blitting so that the blit size is exactly the same
  // as the window.
  if (!UpdateSize()) {
    LOG(ERROR) << "Failed to update size of GLContextOSMesa.";
    return false;
  }

  gfx::Size size = GetSize();

  // Copy the frame into the pixmap.
  XWindowAttributes attributes;
  XGetWindowAttributes(g_osmesa_display, window_, &attributes);
  ui::PutARGBImage(g_osmesa_display,
                   attributes.visual,
                   attributes.depth,
                   pixmap_,
                   pixmap_graphics_context_,
                   static_cast<const uint8*>(GetHandle()),
                   size.width(),
                   size.height());

  // Copy the pixmap to the window.
  XCopyArea(g_osmesa_display,
            pixmap_,
            window_,
            window_graphics_context_,
            0, 0,
            size.width(), size.height(),
            0, 0);

  return true;
}

bool NativeViewGLSurfaceOSMesa::UpdateSize() {
  // Get the window size.
  XWindowAttributes attributes;
  XGetWindowAttributes(g_osmesa_display, window_, &attributes);
  gfx::Size window_size = gfx::Size(std::max(1, attributes.width),
                                    std::max(1, attributes.height));

  // Early out if the size has not changed.
  gfx::Size osmesa_size = GetSize();
  if (pixmap_graphics_context_ && pixmap_ && window_size == osmesa_size)
    return true;

  // Change osmesa surface size to that of window.
  Resize(window_size);

  // Destroy the previous pixmap and graphics context.
  if (pixmap_graphics_context_) {
    XFreeGC(g_osmesa_display, pixmap_graphics_context_);
    pixmap_graphics_context_ = NULL;
  }
  if (pixmap_) {
    XFreePixmap(g_osmesa_display, pixmap_);
    pixmap_ = 0;
  }

  // Recreate a pixmap to hold the frame.
  pixmap_ = XCreatePixmap(g_osmesa_display,
                          window_,
                          window_size.width(),
                          window_size.height(),
                          attributes.depth);
  if (!pixmap_) {
    LOG(ERROR) << "XCreatePixmap failed.";
    return false;
  }

  // Recreate a graphics context for the pixmap.
  pixmap_graphics_context_ = XCreateGC(g_osmesa_display, pixmap_, 0, NULL);
  if (!pixmap_graphics_context_) {
    LOG(ERROR) << "XCreateGC failed";
    return false;
  }

  return true;
}

GLContext* GLContext::CreateViewGLContext(gfx::PluginWindowHandle window,
                                          bool multisampled) {
  switch (GetGLImplementation()) {
    case kGLImplementationDesktopGL: {
      scoped_ptr<GLSurfaceGLX> surface(new NativeViewGLSurfaceGLX(window));
      if (!surface->Initialize()) {
        return NULL;
      }

      scoped_ptr<GLContextGLX> context(
          new GLContextGLX(surface.release()));
      if (!context->Initialize(NULL))
        return NULL;

      return context.release();
    }
    case kGLImplementationEGLGLES2: {
      scoped_ptr<NativeViewGLSurfaceEGL> surface(new NativeViewGLSurfaceEGL(
          window));
      if (!surface->Initialize())
        return NULL;

      scoped_ptr<GLContextEGL> context(
          new GLContextEGL(surface.release()));
      if (!context->Initialize(NULL))
        return NULL;

      return context.release();
    }
    case kGLImplementationOSMesaGL: {
      scoped_ptr<NativeViewGLSurfaceOSMesa> surface(
          new NativeViewGLSurfaceOSMesa(window));
      if (!surface->Initialize())
        return NULL;

      scoped_ptr<GLContextOSMesa> context(
          new GLContextOSMesa(surface.release()));
      if (!context->Initialize(OSMESA_BGRA, NULL))
        return NULL;

      return context.release();
    }
    case kGLImplementationMockGL:
      return new StubGLContext;
    default:
      NOTREACHED();
      return NULL;
  }
}

GLContext* GLContext::CreateOffscreenGLContext(GLContext* shared_context) {
  switch (GetGLImplementation()) {
    case kGLImplementationDesktopGL: {
      scoped_ptr<PbufferGLSurfaceGLX> surface(new PbufferGLSurfaceGLX(
          gfx::Size(1, 1)));
      if (!surface->Initialize())
        return NULL;

      scoped_ptr<GLContextGLX> context(new GLContextGLX(surface.release()));
      if (!context->Initialize(shared_context))
        return NULL;

      return context.release();
    }
    case kGLImplementationEGLGLES2: {
      scoped_ptr<PbufferGLSurfaceEGL> surface(new PbufferGLSurfaceEGL(
          gfx::Size(1, 1)));
      if (!surface->Initialize())
        return NULL;

      scoped_ptr<GLContextEGL> context(new GLContextEGL(surface.release()));
      if (!context->Initialize(shared_context))
        return NULL;

      return context.release();
    }
    case kGLImplementationOSMesaGL: {
      scoped_ptr<GLSurfaceOSMesa> surface(new GLSurfaceOSMesa());
      surface->Resize(gfx::Size(1, 1));

      scoped_ptr<GLContextOSMesa> context(
          new GLContextOSMesa(surface.release()));
      if (!context->Initialize(OSMESA_BGRA, shared_context))
        return NULL;

      return context.release();
    }
    case kGLImplementationMockGL:
      return new StubGLContext;
    default:
      NOTREACHED();
      return NULL;
  }
}

}  // namespace gfx
