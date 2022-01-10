// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_SURFACE_GLX_H_
#define UI_GL_GL_SURFACE_GLX_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/glx.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_surface.h"

using GLXFBConfig = struct __GLXFBConfigRec*;

namespace gfx {
class VSyncProvider;
}

namespace gl {

class GLSurfacePresentationHelper;

// Base class for GLX surfaces.
class GL_EXPORT GLSurfaceGLX : public GLSurface {
 public:
  GLSurfaceGLX();

  GLSurfaceGLX(const GLSurfaceGLX&) = delete;
  GLSurfaceGLX& operator=(const GLSurfaceGLX&) = delete;

  static bool InitializeOneOff();
  static bool InitializeExtensionSettingsOneOff();
  static void ShutdownOneOff();

  // These aren't particularly tied to surfaces, but since we already
  // have the static InitializeOneOff here, it's easiest to reuse its
  // initialization guards.
  static std::string QueryGLXExtensions();
  static const char* GetGLXExtensions();
  static bool HasGLXExtension(const char* name);
  static bool IsCreateContextSupported();
  static bool IsCreateContextRobustnessSupported();
  static bool IsRobustnessVideoMemoryPurgeSupported();
  static bool IsCreateContextProfileSupported();
  static bool IsCreateContextES2ProfileSupported();
  static bool IsTextureFromPixmapSupported();
  static bool IsOMLSyncControlSupported();
  static bool IsEXTSwapControlSupported();
  static bool IsMESASwapControlSupported();

  void* GetDisplay() override;

  // Get the FB config that the surface was created with or NULL if it is not
  // a GLX drawable.
  void* GetConfig() override = 0;

 protected:
  ~GLSurfaceGLX() override;

 private:
  static bool initialized_;
};

// A surface used to render to a view.
class GL_EXPORT NativeViewGLSurfaceGLX : public GLSurfaceGLX {
 public:
  explicit NativeViewGLSurfaceGLX(gfx::AcceleratedWidget window);

  NativeViewGLSurfaceGLX(const NativeViewGLSurfaceGLX&) = delete;
  NativeViewGLSurfaceGLX& operator=(const NativeViewGLSurfaceGLX&) = delete;

  // Implement GLSurfaceGLX.
  bool Initialize(GLSurfaceFormat format) override;
  void Destroy() override;
  bool Resize(const gfx::Size& size,
              float scale_factor,
              const gfx::ColorSpace& color_space,
              bool has_alpha) override;
  bool IsOffscreen() override;
  gfx::SwapResult SwapBuffers(PresentationCallback callback) override;
  gfx::Size GetSize() override;
  void* GetHandle() override;
  bool SupportsPostSubBuffer() override;
  void* GetConfig() override;
  GLSurfaceFormat GetFormat() override;
  gfx::SwapResult PostSubBuffer(int x,
                                int y,
                                int width,
                                int height,
                                PresentationCallback callback) override;
  bool OnMakeCurrent(GLContext* context) override;
  gfx::VSyncProvider* GetVSyncProvider() override;
  void SetVSyncEnabled(bool enabled) override;

 protected:
  ~NativeViewGLSurfaceGLX() override;

  // Handle registering and unregistering for Expose events.
  virtual void RegisterEvents() = 0;
  virtual void UnregisterEvents() = 0;

  // Forwards Expose event to child window.
  void ForwardExposeEvent(const x11::Event& xevent);

  // Checks if event is Expose for child window.
  bool CanHandleEvent(const x11::Event& xevent);

  gfx::AcceleratedWidget window() const {
    return static_cast<gfx::AcceleratedWidget>(window_);
  }

 private:
  // The handle for the drawable to make current or swap.
  uint32_t GetDrawableHandle() const;

  // Window passed in at creation. Always valid.
  gfx::AcceleratedWidget parent_window_;

  // Child window, used to control resizes so that they're in-order with GL.
  x11::Window window_;

  // GLXDrawable for the window.
  x11::Glx::Window glx_window_;

  GLXFBConfig config_;
  gfx::Size size_;

  bool has_swapped_buffers_;

  std::unique_ptr<gfx::VSyncProvider> vsync_provider_;

  std::unique_ptr<GLSurfacePresentationHelper> presentation_helper_;
};

// A surface used to render to an offscreen pbuffer.
class GL_EXPORT UnmappedNativeViewGLSurfaceGLX : public GLSurfaceGLX {
 public:
  explicit UnmappedNativeViewGLSurfaceGLX(const gfx::Size& size);

  UnmappedNativeViewGLSurfaceGLX(const UnmappedNativeViewGLSurfaceGLX&) =
      delete;
  UnmappedNativeViewGLSurfaceGLX& operator=(
      const UnmappedNativeViewGLSurfaceGLX&) = delete;

  // Implement GLSurfaceGLX.
  bool Initialize(GLSurfaceFormat format) override;
  void Destroy() override;
  bool IsOffscreen() override;
  gfx::SwapResult SwapBuffers(PresentationCallback callback) override;
  gfx::Size GetSize() override;
  void* GetHandle() override;
  void* GetConfig() override;
  GLSurfaceFormat GetFormat() override;

 protected:
  ~UnmappedNativeViewGLSurfaceGLX() override;

 private:
  gfx::Size size_;
  GLXFBConfig config_;
  // Unmapped dummy window, used to provide a compatible surface.
  x11::Window window_;

  // GLXDrawable for the window.
  x11::Glx::Window glx_window_;
};

}  // namespace gl

#endif  // UI_GL_GL_SURFACE_GLX_H_
