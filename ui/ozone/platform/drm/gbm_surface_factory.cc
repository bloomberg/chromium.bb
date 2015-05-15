// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gbm_surface_factory.h"

#include <gbm.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "third_party/khronos/EGL/egl.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/ozone/common/egl_util.h"
#include "ui/ozone/platform/drm/gpu/drm_device_manager.h"
#include "ui/ozone/platform/drm/gpu/drm_window.h"
#include "ui/ozone/platform/drm/gpu/gbm_buffer.h"
#include "ui/ozone/platform/drm/gpu/gbm_device.h"
#include "ui/ozone/platform/drm/gpu/gbm_surface.h"
#include "ui/ozone/platform/drm/gpu/gbm_surfaceless.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_controller.h"
#include "ui/ozone/platform/drm/gpu/screen_manager.h"
#include "ui/ozone/public/native_pixmap.h"
#include "ui/ozone/public/overlay_candidates_ozone.h"
#include "ui/ozone/public/ozone_switches.h"
#include "ui/ozone/public/surface_ozone_canvas.h"
#include "ui/ozone/public/surface_ozone_egl.h"

namespace ui {
namespace {

class SingleOverlay : public OverlayCandidatesOzone {
 public:
  SingleOverlay() {}
  ~SingleOverlay() override {}

  void CheckOverlaySupport(OverlaySurfaceCandidateList* candidates) override {
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
      // 0.01 constant chosen to match DCHECKs in gfx::ToNearestRect and avoid
      // that code asserting on quads that we accept.
      if (overlay->plane_z_order > 0 &&
          IsTransformSupported(overlay->transform) &&
          gfx::IsNearestRectWithinDistance(overlay->display_rect, 0.01f)) {
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
    : DrmSurfaceFactory(NULL), allow_surfaceless_(allow_surfaceless) {
}

GbmSurfaceFactory::~GbmSurfaceFactory() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void GbmSurfaceFactory::InitializeGpu(DrmDeviceManager* drm_device_manager,
                                      ScreenManager* screen_manager) {
  drm_device_manager_ = drm_device_manager;
  screen_manager_ = screen_manager;
}

intptr_t GbmSurfaceFactory::GetNativeDisplay() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return EGL_DEFAULT_DISPLAY;
}

const int32* GbmSurfaceFactory::GetEGLSurfaceProperties(
    const int32* desired_list) {
  DCHECK(thread_checker_.CalledOnValidThread());
  static const int32 kConfigAttribs[] = {EGL_BUFFER_SIZE,
                                         32,
                                         EGL_ALPHA_SIZE,
                                         8,
                                         EGL_BLUE_SIZE,
                                         8,
                                         EGL_GREEN_SIZE,
                                         8,
                                         EGL_RED_SIZE,
                                         8,
                                         EGL_RENDERABLE_TYPE,
                                         EGL_OPENGL_ES2_BIT,
                                         EGL_SURFACE_TYPE,
                                         EGL_WINDOW_BIT,
                                         EGL_NONE};

  return kConfigAttribs;
}

bool GbmSurfaceFactory::LoadEGLGLES2Bindings(
    AddGLLibraryCallback add_gl_library,
    SetGLGetProcAddressProcCallback set_gl_get_proc_address) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return LoadDefaultEGLGLES2Bindings(add_gl_library, set_gl_get_proc_address);
}

scoped_ptr<SurfaceOzoneCanvas> GbmSurfaceFactory::CreateCanvasForWidget(
    gfx::AcceleratedWidget widget) {
  DCHECK(thread_checker_.CalledOnValidThread());
  LOG(FATAL) << "Software rendering mode is not supported with GBM platform";
  return nullptr;
}

scoped_ptr<SurfaceOzoneEGL> GbmSurfaceFactory::CreateEGLSurfaceForWidget(
    gfx::AcceleratedWidget widget) {
  DCHECK(thread_checker_.CalledOnValidThread());
  scoped_refptr<GbmDevice> gbm = GetGbmDevice(widget);
  DCHECK(gbm);

  scoped_ptr<GbmSurface> surface(
      new GbmSurface(screen_manager_->GetWindow(widget), gbm));
  if (!surface->Initialize())
    return nullptr;

  return surface.Pass();
}

scoped_ptr<SurfaceOzoneEGL>
GbmSurfaceFactory::CreateSurfacelessEGLSurfaceForWidget(
    gfx::AcceleratedWidget widget) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!allow_surfaceless_)
    return nullptr;

  return make_scoped_ptr(new GbmSurfaceless(screen_manager_->GetWindow(widget),
                                            drm_device_manager_));
}

scoped_refptr<ui::NativePixmap> GbmSurfaceFactory::CreateNativePixmap(
    gfx::AcceleratedWidget widget,
    gfx::Size size,
    BufferFormat format,
    BufferUsage usage) {
  if (usage == MAP)
    return nullptr;

  scoped_refptr<GbmDevice> gbm = GetGbmDevice(widget);
  DCHECK(gbm);

  scoped_refptr<GbmBuffer> buffer =
      GbmBuffer::CreateBuffer(gbm, format, size, true);
  if (!buffer.get())
    return nullptr;

  scoped_refptr<GbmPixmap> pixmap(new GbmPixmap(buffer));
  if (!pixmap->Initialize())
    return nullptr;

  return pixmap;
}

OverlayCandidatesOzone* GbmSurfaceFactory::GetOverlayCandidates(
    gfx::AcceleratedWidget w) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
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
  DCHECK(thread_checker_.CalledOnValidThread());
  scoped_refptr<GbmPixmap> pixmap = static_cast<GbmPixmap*>(buffer.get());
  if (!pixmap.get()) {
    LOG(ERROR) << "ScheduleOverlayPlane passed NULL buffer.";
    return false;
  }
  screen_manager_->GetWindow(widget)->QueueOverlayPlane(
      OverlayPlane(pixmap->buffer(), plane_z_order, plane_transform,
                   display_bounds, crop_rect));
  return true;
}

bool GbmSurfaceFactory::CanShowPrimaryPlaneAsOverlay() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return allow_surfaceless_;
}

bool GbmSurfaceFactory::CanCreateNativePixmap(BufferUsage usage) {
  DCHECK(thread_checker_.CalledOnValidThread());
  switch (usage) {
    case MAP:
      return false;
    case SCANOUT:
      return true;
  }
  NOTREACHED();
  return false;
}

scoped_refptr<GbmDevice> GbmSurfaceFactory::GetGbmDevice(
    gfx::AcceleratedWidget widget) {
  return static_cast<GbmDevice*>(
      drm_device_manager_->GetDrmDevice(widget).get());
}

}  // namespace ui
