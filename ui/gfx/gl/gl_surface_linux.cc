// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gl/gl_surface.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#if !defined(USE_WAYLAND)
#include "third_party/mesa/MesaLib/include/GL/osmesa.h"
#endif
#include "ui/gfx/gl/gl_bindings.h"
#include "ui/gfx/gl/gl_implementation.h"
#include "ui/gfx/gl/gl_surface_egl.h"
#if !defined(USE_WAYLAND)
#include "ui/gfx/gl/gl_surface_glx.h"
#include "ui/gfx/gl/gl_surface_osmesa.h"
#endif
#include "ui/gfx/gl/gl_surface_stub.h"

namespace gfx {

#if !defined(USE_WAYLAND)

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

#endif //  !USE_WAYLAND

bool GLSurface::InitializeOneOffInternal() {
  switch (GetGLImplementation()) {
#if !defined(USE_WAYLAND)
    case kGLImplementationDesktopGL:
      if (!GLSurfaceGLX::InitializeOneOff()) {
        LOG(ERROR) << "GLSurfaceGLX::InitializeOneOff failed.";
        return false;
      }
      break;
    case kGLImplementationOSMesaGL:
      if (!NativeViewGLSurfaceOSMesa::InitializeOneOff()) {
        LOG(ERROR) << "NativeViewGLSurfaceOSMesa::InitializeOneOff failed.";
        return false;
      }
      break;
#endif
    case kGLImplementationEGLGLES2:
      if (!GLSurfaceEGL::InitializeOneOff()) {
        LOG(ERROR) << "GLSurfaceEGL::InitializeOneOff failed.";
        return false;
      }
      break;
    default:
      break;
  }

  return true;
}

#if !defined(USE_WAYLAND)

NativeViewGLSurfaceOSMesa::NativeViewGLSurfaceOSMesa(
    gfx::PluginWindowHandle window)
  : GLSurfaceOSMesa(OSMESA_BGRA, gfx::Size()),
    window_graphics_context_(0),
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
  if (!GLSurfaceOSMesa::Initialize())
    return false;

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

  XWindowAttributes attributes;
  if (!XGetWindowAttributes(g_osmesa_display, window_, &attributes)) {
    LOG(ERROR) << "XGetWindowAttributes failed for window " << window_ << ".";
    return false;
  }

  // Copy the frame into the pixmap.
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
  if (!XGetWindowAttributes(g_osmesa_display, window_, &attributes)) {
    LOG(ERROR) << "XGetWindowAttributes failed for window " << window_ << ".";
    return false;
  }
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

#endif //  !USE_WAYLAND

scoped_refptr<GLSurface> GLSurface::CreateViewGLSurface(
    bool software,
    gfx::PluginWindowHandle window) {
  if (software)
    return NULL;

  switch (GetGLImplementation()) {
#if !defined(USE_WAYLAND)
    case kGLImplementationOSMesaGL: {
      scoped_refptr<GLSurface> surface(
          new NativeViewGLSurfaceOSMesa(window));
      if (!surface->Initialize())
        return NULL;

      return surface;
    }
    case kGLImplementationDesktopGL: {
      scoped_refptr<GLSurface> surface(new NativeViewGLSurfaceGLX(
          window));
      if (!surface->Initialize())
        return NULL;

      return surface;
    }
#endif
    case kGLImplementationEGLGLES2: {
      scoped_refptr<GLSurface> surface(new NativeViewGLSurfaceEGL(
          false, window));
      if (!surface->Initialize())
        return NULL;

      return surface;
    }
    case kGLImplementationMockGL:
      return new GLSurfaceStub;
    default:
      NOTREACHED();
      return NULL;
  }
}

scoped_refptr<GLSurface> GLSurface::CreateOffscreenGLSurface(
    bool software,
    const gfx::Size& size) {
  if (software)
    return NULL;

  switch (GetGLImplementation()) {
#if !defined(USE_WAYLAND)
    case kGLImplementationOSMesaGL: {
      scoped_refptr<GLSurface> surface(new GLSurfaceOSMesa(OSMESA_RGBA,
                                                           size));
      if (!surface->Initialize())
        return NULL;

      return surface;
    }
    case kGLImplementationDesktopGL: {
      scoped_refptr<GLSurface> surface(new PbufferGLSurfaceGLX(size));
      if (!surface->Initialize())
        return NULL;

      return surface;
    }
#endif
    case kGLImplementationEGLGLES2: {
      scoped_refptr<GLSurface> surface(new PbufferGLSurfaceEGL(false, size));
      if (!surface->Initialize())
        return NULL;

      return surface;
    }
    case kGLImplementationMockGL:
      return new GLSurfaceStub;
    default:
      NOTREACHED();
      return NULL;
  }
}

}  // namespace gfx
