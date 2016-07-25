// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/init/gl_surface_ozone.h"

#include <stddef.h>

#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/worker_pool.h"
#include "ui/gl/egl_util.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_surface_osmesa.h"
#include "ui/gl/gl_surface_overlay.h"
#include "ui/gl/gl_surface_stub.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/scoped_make_current.h"
#include "ui/ozone/gl/gl_image_ozone_native_pixmap.h"
#include "ui/ozone/public/native_pixmap.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#include "ui/ozone/public/surface_ozone_egl.h"

namespace gl {

namespace {

// Helper function for base::Bind to create callback to eglChooseConfig.
bool EglChooseConfig(EGLDisplay display,
                     const int32_t* attribs,
                     EGLConfig* configs,
                     int32_t config_size,
                     int32_t* num_configs) {
  return eglChooseConfig(display, attribs, configs, config_size, num_configs);
}

// Helper function for base::Bind to create callback to eglGetConfigAttrib.
bool EglGetConfigAttribute(EGLDisplay display,
                           EGLConfig config,
                           int32_t attribute,
                           int32_t* value) {
  return eglGetConfigAttrib(display, config, attribute, value);
}

// Populates EglConfigCallbacks with appropriate callbacks.
ui::EglConfigCallbacks GetEglConfigCallbacks(EGLDisplay display) {
  ui::EglConfigCallbacks callbacks;
  callbacks.choose_config = base::Bind(EglChooseConfig, display);
  callbacks.get_config_attribute = base::Bind(EglGetConfigAttribute, display);
  callbacks.get_last_error_string = base::Bind(&ui::GetLastEGLErrorString);
  return callbacks;
}

// A thin wrapper around GLSurfaceEGL that owns the EGLNativeWindow.
class GL_EXPORT GLSurfaceOzoneEGL : public NativeViewGLSurfaceEGL {
 public:
  GLSurfaceOzoneEGL(std::unique_ptr<ui::SurfaceOzoneEGL> ozone_surface,
                    gfx::AcceleratedWidget widget);

  // GLSurface:
  bool Initialize(GLSurface::Format format) override;
  bool Resize(const gfx::Size& size,
              float scale_factor,
              bool has_alpha) override;
  gfx::SwapResult SwapBuffers() override;
  bool ScheduleOverlayPlane(int z_order,
                            gfx::OverlayTransform transform,
                            GLImage* image,
                            const gfx::Rect& bounds_rect,
                            const gfx::RectF& crop_rect) override;
  EGLConfig GetConfig() override;

 private:
  using NativeViewGLSurfaceEGL::Initialize;

  ~GLSurfaceOzoneEGL() override;

  bool ReinitializeNativeSurface();

  // The native surface. Deleting this is allowed to free the EGLNativeWindow.
  std::unique_ptr<ui::SurfaceOzoneEGL> ozone_surface_;
  gfx::AcceleratedWidget widget_;

  DISALLOW_COPY_AND_ASSIGN(GLSurfaceOzoneEGL);
};

GLSurfaceOzoneEGL::GLSurfaceOzoneEGL(
    std::unique_ptr<ui::SurfaceOzoneEGL> ozone_surface,
    gfx::AcceleratedWidget widget)
    : NativeViewGLSurfaceEGL(ozone_surface->GetNativeWindow()),
      ozone_surface_(std::move(ozone_surface)),
      widget_(widget) {}

bool GLSurfaceOzoneEGL::Initialize(GLSurface::Format format) {
  format_ = format;
  return Initialize(ozone_surface_->CreateVSyncProvider());
}

bool GLSurfaceOzoneEGL::Resize(const gfx::Size& size,
                               float scale_factor,
                               bool has_alpha) {
  if (!ozone_surface_->ResizeNativeWindow(size)) {
    if (!ReinitializeNativeSurface() ||
        !ozone_surface_->ResizeNativeWindow(size))
      return false;
  }

  return NativeViewGLSurfaceEGL::Resize(size, scale_factor, has_alpha);
}

gfx::SwapResult GLSurfaceOzoneEGL::SwapBuffers() {
  gfx::SwapResult result = NativeViewGLSurfaceEGL::SwapBuffers();
  if (result != gfx::SwapResult::SWAP_ACK)
    return result;

  return ozone_surface_->OnSwapBuffers() ? gfx::SwapResult::SWAP_ACK
                                         : gfx::SwapResult::SWAP_FAILED;
}

bool GLSurfaceOzoneEGL::ScheduleOverlayPlane(int z_order,
                                             gfx::OverlayTransform transform,
                                             GLImage* image,
                                             const gfx::Rect& bounds_rect,
                                             const gfx::RectF& crop_rect) {
  return image->ScheduleOverlayPlane(widget_, z_order, transform, bounds_rect,
                                     crop_rect);
}

EGLConfig GLSurfaceOzoneEGL::GetConfig() {
  if (!config_) {
    ui::EglConfigCallbacks callbacks = GetEglConfigCallbacks(GetDisplay());
    config_ = ozone_surface_->GetEGLSurfaceConfig(callbacks);
  }
  if (config_)
    return config_;
  return NativeViewGLSurfaceEGL::GetConfig();
}

GLSurfaceOzoneEGL::~GLSurfaceOzoneEGL() {
  Destroy();  // The EGL surface must be destroyed before SurfaceOzone.
}

bool GLSurfaceOzoneEGL::ReinitializeNativeSurface() {
  std::unique_ptr<ui::ScopedMakeCurrent> scoped_make_current;
  GLContext* current_context = GLContext::GetCurrent();
  bool was_current = current_context && current_context->IsCurrent(this);
  if (was_current) {
    scoped_make_current.reset(new ui::ScopedMakeCurrent(current_context, this));
  }

  Destroy();
  ozone_surface_ = ui::OzonePlatform::GetInstance()
                       ->GetSurfaceFactoryOzone()
                       ->CreateEGLSurfaceForWidget(widget_);
  if (!ozone_surface_) {
    LOG(ERROR) << "Failed to create native surface.";
    return false;
  }

  window_ = ozone_surface_->GetNativeWindow();
  if (!Initialize(format_)) {
    LOG(ERROR) << "Failed to initialize.";
    return false;
  }

  return true;
}

}  // namespace

scoped_refptr<GLSurface> CreateViewGLSurfaceOzone(
    gfx::AcceleratedWidget window) {
  std::unique_ptr<ui::SurfaceOzoneEGL> surface_ozone =
      ui::OzonePlatform::GetInstance()
          ->GetSurfaceFactoryOzone()
          ->CreateEGLSurfaceForWidget(window);
  if (!surface_ozone)
    return nullptr;
  return InitializeGLSurface(
      new GLSurfaceOzoneEGL(std::move(surface_ozone), window));
}

}  // namespace gl
