// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_SURFACE_EGL_H_
#define UI_GL_GL_SURFACE_EGL_H_

#if defined(OS_WIN)
#include <windows.h>
#endif

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_surface_overlay.h"

namespace gl {

// If adding a new type, also add it to EGLDisplayType in
// tools/metrics/histograms/histograms.xml. Don't remove or reorder entries.
enum DisplayType {
  DEFAULT = 0,
  SWIFT_SHADER = 1,
  ANGLE_WARP = 2,
  ANGLE_D3D9 = 3,
  ANGLE_D3D11 = 4,
  ANGLE_OPENGL = 5,
  ANGLE_OPENGLES = 6,
  ANGLE_NULL = 7,
  DISPLAY_TYPE_MAX = 8,
};

GL_EXPORT void GetEGLInitDisplays(bool supports_angle_d3d,
                                  bool supports_angle_opengl,
                                  bool supports_angle_null,
                                  const base::CommandLine* command_line,
                                  std::vector<DisplayType>* init_displays);

// Interface for EGL surface.
class GL_EXPORT GLSurfaceEGL : public GLSurface {
 public:
  GLSurfaceEGL();

  // Implement GLSurface.
  EGLDisplay GetDisplay() override;
  EGLConfig GetConfig() override;
  GLSurfaceFormat GetFormat() override;

  static bool InitializeOneOff(EGLNativeDisplayType native_display);
  static void ShutdownOneOff();
  static EGLDisplay GetHardwareDisplay();
  static EGLDisplay InitializeDisplay(EGLNativeDisplayType native_display);
  static EGLNativeDisplayType GetNativeDisplay();

  // These aren't particularly tied to surfaces, but since we already
  // have the static InitializeOneOff here, it's easiest to reuse its
  // initialization guards.
  static const char* GetEGLExtensions();
  static bool HasEGLExtension(const char* name);
  static bool IsCreateContextRobustnessSupported();
  static bool IsCreateContextBindGeneratesResourceSupported();
  static bool IsCreateContextWebGLCompatabilitySupported();
  static bool IsEGLSurfacelessContextSupported();
  static bool IsEGLContextPrioritySupported();
  static bool IsDirectCompositionSupported();

 protected:
  ~GLSurfaceEGL() override;

  EGLConfig config_ = nullptr;
  GLSurfaceFormat format_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GLSurfaceEGL);
  static bool initialized_;
};

// Encapsulates an EGL surface bound to a view.
class GL_EXPORT NativeViewGLSurfaceEGL : public GLSurfaceEGL {
 public:
  NativeViewGLSurfaceEGL(EGLNativeWindowType window,
                         std::unique_ptr<gfx::VSyncProvider> vsync_provider);

  // Implement GLSurface.
  using GLSurfaceEGL::Initialize;
  bool Initialize(GLSurfaceFormat format) override;
  void Destroy() override;
  bool Resize(const gfx::Size& size,
              float scale_factor,
              bool has_alpha) override;
  bool Recreate() override;
  bool IsOffscreen() override;
  gfx::SwapResult SwapBuffers() override;
  gfx::Size GetSize() override;
  EGLSurface GetHandle() override;
  bool SupportsPostSubBuffer() override;
  gfx::SwapResult PostSubBuffer(int x, int y, int width, int height) override;
  bool SupportsCommitOverlayPlanes() override;
  gfx::SwapResult CommitOverlayPlanes() override;
  gfx::VSyncProvider* GetVSyncProvider() override;
  bool ScheduleOverlayPlane(int z_order,
                            gfx::OverlayTransform transform,
                            GLImage* image,
                            const gfx::Rect& bounds_rect,
                            const gfx::RectF& crop_rect) override;
  bool FlipsVertically() const override;
  bool BuffersFlipped() const override;

  // Takes care of the platform dependant bits, of any, for creating the window.
  virtual bool InitializeNativeWindow();

 protected:
  ~NativeViewGLSurfaceEGL() override;

  EGLNativeWindowType window_;
  gfx::Size size_;
  bool enable_fixed_size_angle_;

  gfx::SwapResult SwapBuffersWithDamage(const std::vector<int>& rects);

 private:
  // Commit the |pending_overlays_| and clear the vector. Returns false if any
  // fail to be committed.
  bool CommitAndClearPendingOverlays();

  EGLSurface surface_;
  bool supports_post_sub_buffer_;
  bool supports_swap_buffer_with_damage_;
  bool flips_vertically_;

  std::unique_ptr<gfx::VSyncProvider> vsync_provider_external_;
  std::unique_ptr<gfx::VSyncProvider> vsync_provider_internal_;

  std::vector<GLSurfaceOverlay> pending_overlays_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewGLSurfaceEGL);
};

// Encapsulates a pbuffer EGL surface.
class GL_EXPORT PbufferGLSurfaceEGL : public GLSurfaceEGL {
 public:
  explicit PbufferGLSurfaceEGL(const gfx::Size& size);

  // Implement GLSurface.
  bool Initialize(GLSurfaceFormat format) override;
  void Destroy() override;
  bool IsOffscreen() override;
  gfx::SwapResult SwapBuffers() override;
  gfx::Size GetSize() override;
  bool Resize(const gfx::Size& size,
              float scale_factor,
              bool has_alpha) override;
  EGLSurface GetHandle() override;
  void* GetShareHandle() override;

 protected:
  ~PbufferGLSurfaceEGL() override;

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
  bool Initialize(GLSurfaceFormat format) override;
  void Destroy() override;
  bool IsOffscreen() override;
  bool IsSurfaceless() const override;
  gfx::SwapResult SwapBuffers() override;
  gfx::Size GetSize() override;
  bool Resize(const gfx::Size& size,
              float scale_factor,
              bool has_alpha) override;
  EGLSurface GetHandle() override;
  void* GetShareHandle() override;

 protected:
  ~SurfacelessEGL() override;

 private:
  gfx::Size size_;
  DISALLOW_COPY_AND_ASSIGN(SurfacelessEGL);
};

}  // namespace gl

#endif  // UI_GL_GL_SURFACE_EGL_H_
