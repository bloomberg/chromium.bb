// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface.h"

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_surface_osmesa.h"
#include "ui/gl/gl_surface_stub.h"
#include "ui/gl/scoped_make_current.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#include "ui/ozone/public/surface_ozone_egl.h"

namespace gfx {

namespace {

// A thin wrapper around GLSurfaceEGL that owns the EGLNativeWindow
class GL_EXPORT GLSurfaceOzoneEGL : public NativeViewGLSurfaceEGL {
 public:
  GLSurfaceOzoneEGL(scoped_ptr<ui::SurfaceOzoneEGL> ozone_surface,
                    AcceleratedWidget widget)
      : NativeViewGLSurfaceEGL(ozone_surface->GetNativeWindow()),
        ozone_surface_(ozone_surface.Pass()),
        widget_(widget) {}

  bool Initialize() override {
    return Initialize(ozone_surface_->CreateVSyncProvider());
  }
  bool Resize(const gfx::Size& size) override {
    if (!ozone_surface_->ResizeNativeWindow(size)) {
      if (!ReinitializeNativeSurface() ||
          !ozone_surface_->ResizeNativeWindow(size))
        return false;
    }

    return NativeViewGLSurfaceEGL::Resize(size);
  }
  bool SwapBuffers() override {
    if (!NativeViewGLSurfaceEGL::SwapBuffers())
      return false;

    return ozone_surface_->OnSwapBuffers();
  }
  bool ScheduleOverlayPlane(int z_order,
                            OverlayTransform transform,
                            GLImage* image,
                            const Rect& bounds_rect,
                            const RectF& crop_rect) override {
    return image->ScheduleOverlayPlane(
        widget_, z_order, transform, bounds_rect, crop_rect);
  }

 private:
  using NativeViewGLSurfaceEGL::Initialize;

  ~GLSurfaceOzoneEGL() override {
    Destroy();  // EGL surface must be destroyed before SurfaceOzone
  }

  bool ReinitializeNativeSurface() {
    scoped_ptr<ui::ScopedMakeCurrent> scoped_make_current;
    GLContext* current_context = GLContext::GetCurrent();
    bool was_current =
        current_context && current_context->IsCurrent(this);
    if (was_current) {
      scoped_make_current.reset(
          new ui::ScopedMakeCurrent(current_context, this));
    }

    Destroy();
    ozone_surface_ =
        ui::SurfaceFactoryOzone::GetInstance()->CreateEGLSurfaceForWidget(
            widget_).Pass();
    if (!ozone_surface_) {
      LOG(ERROR) << "Failed to create native surface.";
      return false;
    }

    window_ = ozone_surface_->GetNativeWindow();
    if (!Initialize()) {
      LOG(ERROR) << "Failed to initialize.";
      return false;
    }

    return true;
  }

  // The native surface. Deleting this is allowed to free the EGLNativeWindow.
  scoped_ptr<ui::SurfaceOzoneEGL> ozone_surface_;
  AcceleratedWidget widget_;

  DISALLOW_COPY_AND_ASSIGN(GLSurfaceOzoneEGL);
};

class GL_EXPORT GLSurfaceOzoneSurfaceless : public SurfacelessEGL {
 public:
  GLSurfaceOzoneSurfaceless(scoped_ptr<ui::SurfaceOzoneEGL> ozone_surface,
                            AcceleratedWidget widget)
      : SurfacelessEGL(gfx::Size()),
        ozone_surface_(ozone_surface.Pass()),
        widget_(widget),
        has_implicit_external_sync_(
            HasEGLExtension("EGL_ARM_implicit_external_sync")) {}

  bool Initialize() override {
    if (!SurfacelessEGL::Initialize())
      return false;
    vsync_provider_ = ozone_surface_->CreateVSyncProvider();
    if (!vsync_provider_)
      return false;
    return true;
  }
  bool Resize(const gfx::Size& size) override {
    if (!ozone_surface_->ResizeNativeWindow(size))
      return false;

    return SurfacelessEGL::Resize(size);
  }
  bool SwapBuffers() override {
    if (!Flush())
      return false;
    return ozone_surface_->OnSwapBuffers();
  }
  bool ScheduleOverlayPlane(int z_order,
                            OverlayTransform transform,
                            GLImage* image,
                            const Rect& bounds_rect,
                            const RectF& crop_rect) override {
    return image->ScheduleOverlayPlane(
        widget_, z_order, transform, bounds_rect, crop_rect);
  }
  bool IsOffscreen() override { return false; }
  VSyncProvider* GetVSyncProvider() override { return vsync_provider_.get(); }
  bool SupportsPostSubBuffer() override { return true; }
  bool PostSubBuffer(int x, int y, int width, int height) override {
    // The actual sub buffer handling is handled at higher layers.
    SwapBuffers();
    return true;
  }
  bool SwapBuffersAsync(const SwapCompletionCallback& callback) override {
    if (!Flush())
      return false;
    return ozone_surface_->OnSwapBuffersAsync(callback);
  }
  bool PostSubBufferAsync(int x,
                          int y,
                          int width,
                          int height,
                          const SwapCompletionCallback& callback) override {
    return SwapBuffersAsync(callback);
  }

 private:
  ~GLSurfaceOzoneSurfaceless() override {
    Destroy();  // EGL surface must be destroyed before SurfaceOzone
  }

  bool Flush() {
    glFlush();
    // TODO: crbug.com/462360 the following should be replaced by a per surface
    // flush as it gets implemented in GL drivers.
    if (has_implicit_external_sync_) {
      const EGLint attrib_list[] = {
          EGL_SYNC_CONDITION_KHR,
          EGL_SYNC_PRIOR_COMMANDS_IMPLICIT_EXTERNAL_ARM,
          EGL_NONE};
      EGLSyncKHR fence =
          eglCreateSyncKHR(GetDisplay(), EGL_SYNC_FENCE_KHR, attrib_list);
      if (fence) {
        // TODO(dbehr): piman@ suggests we could improve here by moving
        // following wait to right before drmModePageFlip crbug.com/456417.
        eglClientWaitSyncKHR(GetDisplay(), fence,
                             EGL_SYNC_FLUSH_COMMANDS_BIT_KHR, EGL_FOREVER_KHR);
        eglDestroySyncKHR(GetDisplay(), fence);
      } else {
        return false;
      }
    } else if (ozone_surface_->IsUniversalDisplayLinkDevice()) {
      glFinish();
    }
    return true;
  }

  // The native surface. Deleting this is allowed to free the EGLNativeWindow.
  scoped_ptr<ui::SurfaceOzoneEGL> ozone_surface_;
  AcceleratedWidget widget_;
  scoped_ptr<VSyncProvider> vsync_provider_;
  bool has_implicit_external_sync_;
  DISALLOW_COPY_AND_ASSIGN(GLSurfaceOzoneSurfaceless);
};

}  // namespace

// static
bool GLSurface::InitializeOneOffInternal() {
  switch (GetGLImplementation()) {
    case kGLImplementationEGLGLES2:
      if (!GLSurfaceEGL::InitializeOneOff()) {
        LOG(ERROR) << "GLSurfaceEGL::InitializeOneOff failed.";
        return false;
      }

      return true;
    case kGLImplementationOSMesaGL:
    case kGLImplementationMockGL:
      return true;
    default:
      return false;
  }
}

// static
scoped_refptr<GLSurface> GLSurface::CreateViewGLSurface(
    gfx::AcceleratedWidget window) {
  if (GetGLImplementation() == kGLImplementationOSMesaGL) {
    scoped_refptr<GLSurface> surface(new GLSurfaceOSMesaHeadless());
    if (!surface->Initialize())
      return NULL;
    return surface;
  }
  DCHECK(GetGLImplementation() == kGLImplementationEGLGLES2);
  if (window != kNullAcceleratedWidget) {
    scoped_refptr<GLSurface> surface;
    if (GLSurfaceEGL::IsEGLSurfacelessContextSupported() &&
        ui::SurfaceFactoryOzone::GetInstance()
            ->CanShowPrimaryPlaneAsOverlay()) {
      scoped_ptr<ui::SurfaceOzoneEGL> surface_ozone =
          ui::SurfaceFactoryOzone::GetInstance()
              ->CreateSurfacelessEGLSurfaceForWidget(window);
      if (!surface_ozone)
        return NULL;
      surface = new GLSurfaceOzoneSurfaceless(surface_ozone.Pass(), window);
    } else {
      scoped_ptr<ui::SurfaceOzoneEGL> surface_ozone =
          ui::SurfaceFactoryOzone::GetInstance()->CreateEGLSurfaceForWidget(
              window);
      if (!surface_ozone)
        return NULL;

      surface = new GLSurfaceOzoneEGL(surface_ozone.Pass(), window);
    }
    if (!surface->Initialize())
      return NULL;
    return surface;
  } else {
    scoped_refptr<GLSurface> surface = new GLSurfaceStub();
    if (surface->Initialize())
      return surface;
  }
  return NULL;
}

// static
scoped_refptr<GLSurface> GLSurface::CreateOffscreenGLSurface(
    const gfx::Size& size) {
  switch (GetGLImplementation()) {
    case kGLImplementationOSMesaGL: {
      scoped_refptr<GLSurface> surface(
          new GLSurfaceOSMesa(OSMesaSurfaceFormatBGRA, size));
      if (!surface->Initialize())
        return NULL;

      return surface;
    }
    case kGLImplementationEGLGLES2: {
      scoped_refptr<GLSurface> surface;
      if (GLSurfaceEGL::IsEGLSurfacelessContextSupported() &&
          (size.width() == 0 && size.height() == 0)) {
        surface = new SurfacelessEGL(size);
      } else
        surface = new PbufferGLSurfaceEGL(size);

      if (!surface->Initialize())
        return NULL;
      return surface;
    }
    default:
      NOTREACHED();
      return NULL;
  }
}

EGLNativeDisplayType GetPlatformDefaultEGLNativeDisplay() {
  return ui::SurfaceFactoryOzone::GetInstance()->GetNativeDisplay();
}

}  // namespace gfx
