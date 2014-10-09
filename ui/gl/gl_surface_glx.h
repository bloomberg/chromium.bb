// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_SURFACE_GLX_H_
#define UI_GL_GL_SURFACE_GLX_H_

#include <string>

#include "base/compiler_specific.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_surface.h"

namespace gfx {

// Base class for GLX surfaces.
class GL_EXPORT GLSurfaceGLX : public GLSurface {
 public:
  GLSurfaceGLX();

  static bool InitializeOneOff();

  // These aren't particularly tied to surfaces, but since we already
  // have the static InitializeOneOff here, it's easiest to reuse its
  // initialization guards.
  static const char* GetGLXExtensions();
  static bool HasGLXExtension(const char* name);
  static bool IsCreateContextSupported();
  static bool IsCreateContextRobustnessSupported();
  static bool IsTextureFromPixmapSupported();
  static bool IsOMLSyncControlSupported();

  virtual void* GetDisplay() override;

  // Get the FB config that the surface was created with or NULL if it is not
  // a GLX drawable.
  virtual void* GetConfig() = 0;

 protected:
  virtual ~GLSurfaceGLX();

 private:
  DISALLOW_COPY_AND_ASSIGN(GLSurfaceGLX);
};

// A surface used to render to a view.
class GL_EXPORT NativeViewGLSurfaceGLX : public GLSurfaceGLX,
                                         public ui::PlatformEventDispatcher {
 public:
  explicit NativeViewGLSurfaceGLX(gfx::AcceleratedWidget window);

  // Implement GLSurfaceGLX.
  virtual bool Initialize() override;
  virtual void Destroy() override;
  virtual bool Resize(const gfx::Size& size) override;
  virtual bool IsOffscreen() override;
  virtual bool SwapBuffers() override;
  virtual gfx::Size GetSize() override;
  virtual void* GetHandle() override;
  virtual bool SupportsPostSubBuffer() override;
  virtual void* GetConfig() override;
  virtual bool PostSubBuffer(int x, int y, int width, int height) override;
  virtual VSyncProvider* GetVSyncProvider() override;

 protected:
  virtual ~NativeViewGLSurfaceGLX();

 private:
  // The handle for the drawable to make current or swap.
  gfx::AcceleratedWidget GetDrawableHandle() const;

  // PlatformEventDispatcher implementation
  virtual bool CanDispatchEvent(const ui::PlatformEvent& event) override;
  virtual uint32_t DispatchEvent(const ui::PlatformEvent& event) override;

  // Window passed in at creation. Always valid.
  gfx::AcceleratedWidget parent_window_;

  // Child window, used to control resizes so that they're in-order with GL.
  gfx::AcceleratedWidget window_;

  void* config_;
  gfx::Size size_;

  scoped_ptr<VSyncProvider> vsync_provider_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewGLSurfaceGLX);
};

// A surface used to render to an offscreen pbuffer.
class GL_EXPORT PbufferGLSurfaceGLX : public GLSurfaceGLX {
 public:
  explicit PbufferGLSurfaceGLX(const gfx::Size& size);

  // Implement GLSurfaceGLX.
  virtual bool Initialize() override;
  virtual void Destroy() override;
  virtual bool IsOffscreen() override;
  virtual bool SwapBuffers() override;
  virtual gfx::Size GetSize() override;
  virtual void* GetHandle() override;
  virtual void* GetConfig() override;

 protected:
  virtual ~PbufferGLSurfaceGLX();

 private:
  gfx::Size size_;
  void* config_;
  XID pbuffer_;

  DISALLOW_COPY_AND_ASSIGN(PbufferGLSurfaceGLX);
};

}  // namespace gfx

#endif  // UI_GL_GL_SURFACE_GLX_H_
