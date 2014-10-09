// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_SURFACE_EGL_H_
#define UI_GL_GL_SURFACE_EGL_H_

#if defined(OS_WIN)
#include <windows.h>
#endif

#include <string>

#include "base/compiler_specific.h"
#include "base/time/time.h"
#include "ui/gfx/size.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_surface.h"

namespace gfx {

// Get default EGL display for GLSurfaceEGL (differs by platform).
EGLNativeDisplayType GetPlatformDefaultEGLNativeDisplay();

// Interface for EGL surface.
class GL_EXPORT GLSurfaceEGL : public GLSurface {
 public:
  GLSurfaceEGL();

  // Implement GLSurface.
  virtual EGLDisplay GetDisplay() override;

  static bool InitializeOneOff();
  static EGLDisplay GetHardwareDisplay();
  static EGLNativeDisplayType GetNativeDisplay();

  // These aren't particularly tied to surfaces, but since we already
  // have the static InitializeOneOff here, it's easiest to reuse its
  // initialization guards.
  static const char* GetEGLExtensions();
  static bool HasEGLExtension(const char* name);
  static bool IsCreateContextRobustnessSupported();
  static bool IsEGLSurfacelessContextSupported();

 protected:
  virtual ~GLSurfaceEGL();

 private:
#if defined(OS_WIN)
  static EGLDisplay GetPlatformDisplay(EGLNativeDisplayType native_display);
#endif

  DISALLOW_COPY_AND_ASSIGN(GLSurfaceEGL);
};

// Encapsulates an EGL surface bound to a view.
class GL_EXPORT NativeViewGLSurfaceEGL : public GLSurfaceEGL {
 public:
  explicit NativeViewGLSurfaceEGL(EGLNativeWindowType window);

  // Implement GLSurface.
  virtual EGLConfig GetConfig() override;
  virtual bool Initialize() override;
  virtual void Destroy() override;
  virtual bool Resize(const gfx::Size& size) override;
  virtual bool Recreate() override;
  virtual bool IsOffscreen() override;
  virtual bool SwapBuffers() override;
  virtual gfx::Size GetSize() override;
  virtual EGLSurface GetHandle() override;
  virtual bool SupportsPostSubBuffer() override;
  virtual bool PostSubBuffer(int x, int y, int width, int height) override;
  virtual VSyncProvider* GetVSyncProvider() override;

  // Create a NativeViewGLSurfaceEGL with an externally provided VSyncProvider.
  // Takes ownership of the VSyncProvider.
  virtual bool Initialize(scoped_ptr<VSyncProvider> sync_provider);

 protected:
  virtual ~NativeViewGLSurfaceEGL();

  EGLNativeWindowType window_;

 private:
  EGLSurface surface_;
  bool supports_post_sub_buffer_;
  EGLConfig config_;
  gfx::Size size_;

  scoped_ptr<VSyncProvider> vsync_provider_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewGLSurfaceEGL);
};

// Encapsulates a pbuffer EGL surface.
class GL_EXPORT PbufferGLSurfaceEGL : public GLSurfaceEGL {
 public:
  explicit PbufferGLSurfaceEGL(const gfx::Size& size);

  // Implement GLSurface.
  virtual EGLConfig GetConfig() override;
  virtual bool Initialize() override;
  virtual void Destroy() override;
  virtual bool IsOffscreen() override;
  virtual bool SwapBuffers() override;
  virtual gfx::Size GetSize() override;
  virtual bool Resize(const gfx::Size& size) override;
  virtual EGLSurface GetHandle() override;
  virtual void* GetShareHandle() override;

 protected:
  virtual ~PbufferGLSurfaceEGL();

 private:
  gfx::Size size_;
  EGLSurface surface_;

  DISALLOW_COPY_AND_ASSIGN(PbufferGLSurfaceEGL);
};

// SurfacelessEGL is used as Offscreen surface when platform supports
// KHR_surfaceless_context and GL_OES_surfaceless_context. This would avoid the
// need to create a dummy EGLsurface in case we render to client API targets.
class GL_EXPORT SurfacelessEGL : public GLSurfaceEGL {
 public:
  explicit SurfacelessEGL(const gfx::Size& size);

  // Implement GLSurface.
  virtual EGLConfig GetConfig() override;
  virtual bool Initialize() override;
  virtual void Destroy() override;
  virtual bool IsOffscreen() override;
  virtual bool SwapBuffers() override;
  virtual gfx::Size GetSize() override;
  virtual bool Resize(const gfx::Size& size) override;
  virtual EGLSurface GetHandle() override;
  virtual void* GetShareHandle() override;

 protected:
  virtual ~SurfacelessEGL();

 private:
  gfx::Size size_;
  DISALLOW_COPY_AND_ASSIGN(SurfacelessEGL);
};

}  // namespace gfx

#endif  // UI_GL_GL_SURFACE_EGL_H_
