// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_SURFACE_GLX_H_
#define UI_GL_GL_SURFACE_GLX_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gpu_timing.h"

namespace gl {

// Base class for GLX surfaces.
class GL_EXPORT GLSurfaceGLX : public GLSurface {
 public:
  GLSurfaceGLX();

  static bool InitializeOneOff();
  static bool InitializeExtensionSettingsOneOff();
  static void ShutdownOneOff();

  // These aren't particularly tied to surfaces, but since we already
  // have the static InitializeOneOff here, it's easiest to reuse its
  // initialization guards.
  static const char* GetGLXExtensions();
  static bool HasGLXExtension(const char* name);
  static bool IsCreateContextSupported();
  static bool IsCreateContextRobustnessSupported();
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

  unsigned long GetCompatibilityKey() override = 0;

 protected:
  ~GLSurfaceGLX() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(GLSurfaceGLX);
  static bool initialized_;
};

// A surface used to render to a view.
class GL_EXPORT NativeViewGLSurfaceGLX : public GLSurfaceGLX {
 public:
  explicit NativeViewGLSurfaceGLX(gfx::AcceleratedWidget window);

  // Implement GLSurfaceGLX.
  bool Initialize(GLSurfaceFormat format) override;
  void Destroy() override;
  bool Resize(const gfx::Size& size,
              float scale_factor,
              ColorSpace color_space,
              bool has_alpha) override;
  bool IsOffscreen() override;
  gfx::SwapResult SwapBuffers(const PresentationCallback& callback) override;
  gfx::Size GetSize() override;
  void* GetHandle() override;
  bool SupportsPresentationCallback() override;
  bool SupportsPostSubBuffer() override;
  void* GetConfig() override;
  GLSurfaceFormat GetFormat() override;
  unsigned long GetCompatibilityKey() override;
  gfx::SwapResult PostSubBuffer(int x,
                                int y,
                                int width,
                                int height,
                                const PresentationCallback& callback) override;
  bool OnMakeCurrent(GLContext* context) override;
  gfx::VSyncProvider* GetVSyncProvider() override;

  VisualID GetVisualID() const { return visual_id_; }

 protected:
  ~NativeViewGLSurfaceGLX() override;

  // Handle registering and unregistering for Expose events.
  virtual void RegisterEvents() = 0;
  virtual void UnregisterEvents() = 0;

  // Forwards Expose event to child window.
  void ForwardExposeEvent(XEvent* xevent);

  // Checks if event is Expose for child window.
  bool CanHandleEvent(XEvent* xevent);

  gfx::AcceleratedWidget window() const { return window_; }

 private:
  // The handle for the drawable to make current or swap.
  GLXDrawable GetDrawableHandle() const;

  void PreSwapBuffers(const PresentationCallback& callback);
  void PostSwapBuffers();

  // Check |pending_frames_| and run presentation callbacks.
  void CheckPendingFrames();

  // Callback used by PostDelayedTask for running CheckPendingFrames().
  void CheckPendingFramesCallback();

  void UpdateVSyncCallback(const base::TimeTicks timebase,
                           const base::TimeDelta interval);

  // Window passed in at creation. Always valid.
  gfx::AcceleratedWidget parent_window_;

  // Child window, used to control resizes so that they're in-order with GL.
  gfx::AcceleratedWidget window_;

  // GLXDrawable for the window.
  GLXWindow glx_window_;

  GLXFBConfig config_;
  gfx::Size size_;
  VisualID visual_id_;

  std::unique_ptr<gfx::VSyncProvider> vsync_provider_;

  scoped_refptr<GLContext> gl_context_;
  scoped_refptr<GPUTimingClient> gpu_timing_client_;
  struct Frame {
    Frame(Frame&& other);
    Frame(std::unique_ptr<GPUTimer>&& timer,
          const PresentationCallback& callback);
    ~Frame();
    Frame& operator=(Frame&& other);
    std::unique_ptr<GPUTimer> timer;
    PresentationCallback callback;
  };
  base::circular_deque<Frame> pending_frames_;

  base::TimeTicks vsync_timebase_;
  base::TimeDelta vsync_interval_;

  bool waiting_for_vsync_parameters_;

  base::WeakPtrFactory<NativeViewGLSurfaceGLX> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewGLSurfaceGLX);
};

// A surface used to render to an offscreen pbuffer.
class GL_EXPORT UnmappedNativeViewGLSurfaceGLX : public GLSurfaceGLX {
 public:
  explicit UnmappedNativeViewGLSurfaceGLX(const gfx::Size& size);

  // Implement GLSurfaceGLX.
  bool Initialize(GLSurfaceFormat format) override;
  void Destroy() override;
  bool IsOffscreen() override;
  gfx::SwapResult SwapBuffers(const PresentationCallback& callback) override;
  gfx::Size GetSize() override;
  void* GetHandle() override;
  void* GetConfig() override;
  GLSurfaceFormat GetFormat() override;
  unsigned long GetCompatibilityKey() override;

 protected:
  ~UnmappedNativeViewGLSurfaceGLX() override;

 private:
  gfx::Size size_;
  GLXFBConfig config_;
  // Unmapped dummy window, used to provide a compatible surface.
  gfx::AcceleratedWidget window_;

  // GLXDrawable for the window.
  GLXWindow glx_window_;

  DISALLOW_COPY_AND_ASSIGN(UnmappedNativeViewGLSurfaceGLX);
};

}  // namespace gl

#endif  // UI_GL_GL_SURFACE_GLX_H_
