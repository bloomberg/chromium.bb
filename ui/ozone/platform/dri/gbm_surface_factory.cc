// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/gbm_surface_factory.h"

#include <gbm.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "third_party/khronos/EGL/egl.h"
#include "ui/ozone/platform/dri/dri_window_delegate_impl.h"
#include "ui/ozone/platform/dri/dri_window_delegate_manager.h"
#include "ui/ozone/platform/dri/gbm_buffer.h"
#include "ui/ozone/platform/dri/gbm_surface.h"
#include "ui/ozone/platform/dri/gbm_surfaceless.h"
#include "ui/ozone/platform/dri/screen_manager.h"
#include "ui/ozone/public/native_pixmap.h"
#include "ui/ozone/public/overlay_candidates_ozone.h"
#include "ui/ozone/public/ozone_switches.h"
#include "ui/ozone/public/surface_ozone_egl.h"

namespace ui {
namespace {

class SingleOverlay : public OverlayCandidatesOzone {
 public:
  SingleOverlay() {}
  virtual ~SingleOverlay() {}

  virtual void CheckOverlaySupport(
      OverlaySurfaceCandidateList* candidates) OVERRIDE {
    if (candidates->size() == 2) {
      OverlayCandidatesOzone::OverlaySurfaceCandidate* first =
          &(*candidates)[0];
      OverlayCandidatesOzone::OverlaySurfaceCandidate* second =
          &(*candidates)[1];
      OverlayCandidatesOzone::OverlaySurfaceCandidate* overlay;
      if (first->plane_z_order == 0) {
        overlay = second;
      } else if (second->plane_z_order == 0) {
        overlay = first;
      } else {
        NOTREACHED();
        return;
      }
      if (overlay->plane_z_order > 0 &&
          IsTransformSupported(overlay->transform)) {
        overlay->overlay_handled = true;
      }
    }
  }

 private:
  bool IsTransformSupported(gfx::OverlayTransform transform) {
    switch (transform) {
      case gfx::OVERLAY_TRANSFORM_NONE:
        return true;
      default:
        return false;
    }
  }

  DISALLOW_COPY_AND_ASSIGN(SingleOverlay);
};

}  // namespace

GbmSurfaceFactory::GbmSurfaceFactory(bool allow_surfaceless)
    : DriSurfaceFactory(NULL, NULL, NULL),
      device_(NULL),
      allow_surfaceless_(allow_surfaceless) {
}

GbmSurfaceFactory::~GbmSurfaceFactory() {}

void GbmSurfaceFactory::InitializeGpu(
    DriWrapper* dri,
    gbm_device* device,
    ScreenManager* screen_manager,
    DriWindowDelegateManager* window_manager) {
  drm_ = dri;
  device_ = device;
  screen_manager_ = screen_manager;
  window_manager_ = window_manager;
}

intptr_t GbmSurfaceFactory::GetNativeDisplay() {
  DCHECK(state_ == INITIALIZED);
  return reinterpret_cast<intptr_t>(device_);
}

const int32* GbmSurfaceFactory::GetEGLSurfaceProperties(
    const int32* desired_list) {
  static const int32 kConfigAttribs[] = {
    EGL_BUFFER_SIZE, 32,
    EGL_ALPHA_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_RED_SIZE, 8,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
    EGL_NONE
  };

  return kConfigAttribs;
}

bool GbmSurfaceFactory::LoadEGLGLES2Bindings(
      AddGLLibraryCallback add_gl_library,
      SetGLGetProcAddressProcCallback set_gl_get_proc_address) {
  base::NativeLibraryLoadError error;
  base::NativeLibrary gles_library = base::LoadNativeLibrary(
      base::FilePath("libGLESv2.so.2"),
      &error);
  if (!gles_library) {
    LOG(WARNING) << "Failed to load GLES library: " << error.ToString();
    return false;
  }

  base::NativeLibrary egl_library = base::LoadNativeLibrary(
      base::FilePath("libEGL.so.1"),
      &error);
  if (!egl_library) {
    LOG(WARNING) << "Failed to load EGL library: " << error.ToString();
    base::UnloadNativeLibrary(gles_library);
    return false;
  }

  GLGetProcAddressProc get_proc_address =
      reinterpret_cast<GLGetProcAddressProc>(
          base::GetFunctionPointerFromNativeLibrary(
              egl_library, "eglGetProcAddress"));
  if (!get_proc_address) {
    LOG(ERROR) << "eglGetProcAddress not found.";
    base::UnloadNativeLibrary(egl_library);
    base::UnloadNativeLibrary(gles_library);
    return false;
  }

  set_gl_get_proc_address.Run(get_proc_address);
  add_gl_library.Run(egl_library);
  add_gl_library.Run(gles_library);

  return true;
}

scoped_ptr<SurfaceOzoneEGL> GbmSurfaceFactory::CreateEGLSurfaceForWidget(
    gfx::AcceleratedWidget widget) {
  DCHECK(state_ == INITIALIZED);

  DriWindowDelegate* delegate = GetOrCreateWindowDelegate(widget);

  scoped_ptr<GbmSurface> surface(new GbmSurface(delegate, device_, drm_));
  if (!surface->Initialize())
    return scoped_ptr<SurfaceOzoneEGL>();

  return surface.PassAs<SurfaceOzoneEGL>();
}

scoped_ptr<SurfaceOzoneEGL>
GbmSurfaceFactory::CreateSurfacelessEGLSurfaceForWidget(
    gfx::AcceleratedWidget widget) {
  if (!allow_surfaceless_)
    return scoped_ptr<SurfaceOzoneEGL>();

  DriWindowDelegate* delegate = GetOrCreateWindowDelegate(widget);
  return scoped_ptr<SurfaceOzoneEGL>(new GbmSurfaceless(delegate));
}

scoped_refptr<ui::NativePixmap> GbmSurfaceFactory::CreateNativePixmap(
    gfx::Size size,
    BufferFormat format) {
  scoped_refptr<GbmBuffer> buffer = GbmBuffer::CreateBuffer(
      drm_, device_, format, size, true);
  if (!buffer.get())
    return NULL;

  return scoped_refptr<GbmPixmap>(new GbmPixmap(buffer));
}

OverlayCandidatesOzone* GbmSurfaceFactory::GetOverlayCandidates(
    gfx::AcceleratedWidget w) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kOzoneTestSingleOverlaySupport))
    return new SingleOverlay();
  return NULL;
}

bool GbmSurfaceFactory::ScheduleOverlayPlane(
    gfx::AcceleratedWidget widget,
    int plane_z_order,
    gfx::OverlayTransform plane_transform,
    scoped_refptr<NativePixmap> buffer,
    const gfx::Rect& display_bounds,
    const gfx::RectF& crop_rect) {
  scoped_refptr<GbmPixmap> pixmap = static_cast<GbmPixmap*>(buffer.get());
  if (!pixmap.get()) {
    LOG(ERROR) << "ScheduleOverlayPlane passed NULL buffer.";
    return false;
  }
  HardwareDisplayController* hdc =
      window_manager_->GetWindowDelegate(widget)->GetController();
  if (!hdc)
    return true;

  hdc->QueueOverlayPlane(OverlayPlane(pixmap->buffer(),
                                      plane_z_order,
                                      plane_transform,
                                      display_bounds,
                                      crop_rect));
  return true;
}

bool GbmSurfaceFactory::CanShowPrimaryPlaneAsOverlay() {
  return allow_surfaceless_;
}

DriWindowDelegate* GbmSurfaceFactory::GetOrCreateWindowDelegate(
    gfx::AcceleratedWidget widget) {
  if (!window_manager_->HasWindowDelegate(widget)) {
    scoped_ptr<DriWindowDelegate> delegate(
        new DriWindowDelegateImpl(widget, screen_manager_));
    delegate->Initialize();
    window_manager_->AddWindowDelegate(widget, delegate.Pass());
  }

  return window_manager_->GetWindowDelegate(widget);
}

}  // namespace ui
