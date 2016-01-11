// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface.h"

#include <dwmapi.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/trace_event/trace_event.h"
#include "base/win/windows_version.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_surface_osmesa.h"
#include "ui/gl/gl_surface_stub.h"
#include "ui/gl/gl_surface_wgl.h"
#include "ui/gl/vsync_provider_win.h"

// From ANGLE's egl/eglext.h.
#if !defined(EGL_D3D11_ELSE_D3D9_DISPLAY_ANGLE)
#define EGL_D3D11_ELSE_D3D9_DISPLAY_ANGLE \
  reinterpret_cast<EGLNativeDisplayType>(-2)
#endif

namespace gfx {

// This OSMesa GL surface can use GDI to swap the contents of the buffer to a
// view.
class NativeViewGLSurfaceOSMesa : public GLSurfaceOSMesa {
 public:
  explicit NativeViewGLSurfaceOSMesa(gfx::AcceleratedWidget window);

  // Implement subset of GLSurface.
  bool Initialize() override;
  void Destroy() override;
  bool IsOffscreen() override;
  gfx::SwapResult SwapBuffers() override;
  bool SupportsPostSubBuffer() override;
  gfx::SwapResult PostSubBuffer(int x, int y, int width, int height) override;

 private:
  ~NativeViewGLSurfaceOSMesa() override;

  gfx::AcceleratedWidget window_;
  HDC device_context_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewGLSurfaceOSMesa);
};

// Helper routine that does one-off initialization like determining the
// pixel format.
bool GLSurface::InitializeOneOffInternal() {
  switch (GetGLImplementation()) {
    case kGLImplementationDesktopGL:
      if (!GLSurfaceWGL::InitializeOneOff()) {
        LOG(ERROR) << "GLSurfaceWGL::InitializeOneOff failed.";
        return false;
      }
      break;
    case kGLImplementationEGLGLES2:
      if (!GLSurfaceEGL::InitializeOneOff()) {
        LOG(ERROR) << "GLSurfaceEGL::InitializeOneOff failed.";
        return false;
      }
      break;
    case kGLImplementationNone:
    case kGLImplementationDesktopGLCoreProfile:
    case kGLImplementationAppleGL:
      NOTREACHED();
    case kGLImplementationOSMesaGL:
    case kGLImplementationMockGL:
      break;
  }
  return true;
}

NativeViewGLSurfaceOSMesa::NativeViewGLSurfaceOSMesa(
    gfx::AcceleratedWidget window)
    : GLSurfaceOSMesa(OSMesaSurfaceFormatRGBA, gfx::Size(1, 1)),
      window_(window),
      device_context_(NULL) {
  DCHECK(window);
}

NativeViewGLSurfaceOSMesa::~NativeViewGLSurfaceOSMesa() {
  Destroy();
}

bool NativeViewGLSurfaceOSMesa::Initialize() {
  if (!GLSurfaceOSMesa::Initialize())
    return false;

  device_context_ = GetDC(window_);
  return true;
}

void NativeViewGLSurfaceOSMesa::Destroy() {
  if (window_ && device_context_)
    ReleaseDC(window_, device_context_);

  device_context_ = NULL;

  GLSurfaceOSMesa::Destroy();
}

bool NativeViewGLSurfaceOSMesa::IsOffscreen() {
  return false;
}

gfx::SwapResult NativeViewGLSurfaceOSMesa::SwapBuffers() {
  DCHECK(device_context_);

  gfx::Size size = GetSize();

  // Note: negating the height below causes GDI to treat the bitmap data as row
  // 0 being at the top.
  BITMAPV4HEADER info = { sizeof(BITMAPV4HEADER) };
  info.bV4Width = size.width();
  info.bV4Height = -size.height();
  info.bV4Planes = 1;
  info.bV4BitCount = 32;
  info.bV4V4Compression = BI_BITFIELDS;
  info.bV4RedMask = 0x000000FF;
  info.bV4GreenMask = 0x0000FF00;
  info.bV4BlueMask = 0x00FF0000;
  info.bV4AlphaMask = 0xFF000000;

  // Copy the back buffer to the window's device context. Do not check whether
  // StretchDIBits succeeds or not. It will fail if the window has been
  // destroyed but it is preferable to allow rendering to silently fail if the
  // window is destroyed. This is because the primary application of this
  // class of GLContext is for testing and we do not want every GL related ui /
  // browser test to become flaky if there is a race condition between GL
  // context destruction and window destruction.
  StretchDIBits(device_context_,
                0, 0, size.width(), size.height(),
                0, 0, size.width(), size.height(),
                GetHandle(),
                reinterpret_cast<BITMAPINFO*>(&info),
                DIB_RGB_COLORS,
                SRCCOPY);

  return gfx::SwapResult::SWAP_ACK;
}

bool NativeViewGLSurfaceOSMesa::SupportsPostSubBuffer() {
  return true;
}

gfx::SwapResult NativeViewGLSurfaceOSMesa::PostSubBuffer(int x,
                                                         int y,
                                                         int width,
                                                         int height) {
  DCHECK(device_context_);

  gfx::Size size = GetSize();

  // Note: negating the height below causes GDI to treat the bitmap data as row
  // 0 being at the top.
  BITMAPV4HEADER info = { sizeof(BITMAPV4HEADER) };
  info.bV4Width = size.width();
  info.bV4Height = -size.height();
  info.bV4Planes = 1;
  info.bV4BitCount = 32;
  info.bV4V4Compression = BI_BITFIELDS;
  info.bV4RedMask = 0x000000FF;
  info.bV4GreenMask = 0x0000FF00;
  info.bV4BlueMask = 0x00FF0000;
  info.bV4AlphaMask = 0xFF000000;

  // Copy the back buffer to the window's device context. Do not check whether
  // StretchDIBits succeeds or not. It will fail if the window has been
  // destroyed but it is preferable to allow rendering to silently fail if the
  // window is destroyed. This is because the primary application of this
  // class of GLContext is for testing and we do not want every GL related ui /
  // browser test to become flaky if there is a race condition between GL
  // context destruction and window destruction.
  StretchDIBits(device_context_,
                x, size.height() - y - height, width, height,
                x, y, width, height,
                GetHandle(),
                reinterpret_cast<BITMAPINFO*>(&info),
                DIB_RGB_COLORS,
                SRCCOPY);

  return gfx::SwapResult::SWAP_ACK;
}

scoped_refptr<GLSurface> GLSurface::CreateViewGLSurface(
    gfx::AcceleratedWidget window) {
  TRACE_EVENT0("gpu", "GLSurface::CreateViewGLSurface");
  switch (GetGLImplementation()) {
    case kGLImplementationOSMesaGL: {
      scoped_refptr<GLSurface> surface(
          new NativeViewGLSurfaceOSMesa(window));
      if (!surface->Initialize())
        return NULL;

      return surface;
    }
    case kGLImplementationEGLGLES2: {
      DCHECK(window != gfx::kNullAcceleratedWidget);
      scoped_refptr<NativeViewGLSurfaceEGL> surface(
          new NativeViewGLSurfaceEGL(window));
      scoped_ptr<VSyncProvider> sync_provider;
      sync_provider.reset(new VSyncProviderWin(window));
      if (!surface->Initialize(std::move(sync_provider)))
        return NULL;

      return surface;
    }
    case kGLImplementationDesktopGL: {
      scoped_refptr<GLSurface> surface(new NativeViewGLSurfaceWGL(
          window));
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
    const gfx::Size& size) {
  TRACE_EVENT0("gpu", "GLSurface::CreateOffscreenGLSurface");
  switch (GetGLImplementation()) {
    case kGLImplementationOSMesaGL: {
      scoped_refptr<GLSurface> surface(
          new GLSurfaceOSMesa(OSMesaSurfaceFormatRGBA, size));
      if (!surface->Initialize())
        return NULL;

      return surface;
    }
    case kGLImplementationEGLGLES2: {
      scoped_refptr<GLSurface> surface(new PbufferGLSurfaceEGL(size));
      if (!surface->Initialize())
        return NULL;

      return surface;
    }
    case kGLImplementationDesktopGL: {
      scoped_refptr<GLSurface> surface(new PbufferGLSurfaceWGL(size));
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

EGLNativeDisplayType GetPlatformDefaultEGLNativeDisplay() {
  return GetDC(NULL);
}

}  // namespace gfx
