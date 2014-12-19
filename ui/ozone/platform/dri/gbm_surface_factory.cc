// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/gbm_surface_factory.h"

#include <gbm.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "third_party/khronos/EGL/egl.h"
#include "ui/ozone/common/egl_util.h"
#include "ui/ozone/platform/dri/dri_window_delegate_impl.h"
#include "ui/ozone/platform/dri/dri_window_delegate_manager.h"
#include "ui/ozone/platform/dri/dri_wrapper.h"
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
    : DriSurfaceFactory(NULL, NULL),
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
  return reinterpret_cast<intptr_t>(device_);
}

int GbmSurfaceFactory::GetDrmFd() {
  DCHECK(drm_);
  return drm_->get_fd();
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
  return LoadDefaultEGLGLES2Bindings(add_gl_library, set_gl_get_proc_address);
}

scoped_ptr<SurfaceOzoneEGL> GbmSurfaceFactory::CreateEGLSurfaceForWidget(
    gfx::AcceleratedWidget widget) {
  DriWindowDelegate* delegate = GetOrCreateWindowDelegate(widget);

  scoped_ptr<GbmSurface> surface(new GbmSurface(delegate, device_, drm_));
  if (!surface->Initialize())
    return nullptr;

  return surface.Pass();
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
    gfx::AcceleratedWidget widget,
    gfx::Size size,
    BufferFormat format,
    BufferUsage usage) {
  if (usage == MAP)
    return NULL;

  scoped_refptr<GbmBuffer> buffer =
      GbmBuffer::CreateBuffer(drm_, device_, format, size, true);
  if (!buffer.get())
    return NULL;

  scoped_refptr<GbmPixmap> pixmap(new GbmPixmap(buffer));
  if (!pixmap->Initialize(drm_))
    return NULL;

  return pixmap;
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

bool GbmSurfaceFactory::CanCreateNativePixmap(BufferUsage usage) {
  switch (usage) {
    case MAP:
      return false;
    case SCANOUT:
      return true;
  }
  NOTREACHED();
  return false;
}

DriWindowDelegate* GbmSurfaceFactory::GetOrCreateWindowDelegate(
    gfx::AcceleratedWidget widget) {
  if (!window_manager_->HasWindowDelegate(widget)) {
    scoped_ptr<DriWindowDelegate> delegate(new DriWindowDelegateImpl(
        widget, drm_, window_manager_, screen_manager_));
    delegate->Initialize();
    window_manager_->AddWindowDelegate(widget, delegate.Pass());
  }

  return window_manager_->GetWindowDelegate(widget);
}

}  // namespace ui
