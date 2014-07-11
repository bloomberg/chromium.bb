// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/gbm_surface_factory.h"

#include <EGL/egl.h>
#include <gbm.h>

#include "base/files/file_path.h"
#include "ui/ozone/platform/dri/buffer_data.h"
#include "ui/ozone/platform/dri/dri_vsync_provider.h"
#include "ui/ozone/platform/dri/gbm_buffer.h"
#include "ui/ozone/platform/dri/gbm_surface.h"
#include "ui/ozone/platform/dri/hardware_display_controller.h"
#include "ui/ozone/platform/dri/scanout_surface.h"
#include "ui/ozone/platform/dri/screen_manager.h"
#include "ui/ozone/public/native_pixmap.h"
#include "ui/ozone/public/surface_ozone_egl.h"

namespace ui {

namespace {

class GbmSurfaceAdapter : public ui::SurfaceOzoneEGL {
 public:
  GbmSurfaceAdapter(const base::WeakPtr<HardwareDisplayController>& controller);
  virtual ~GbmSurfaceAdapter();

  // SurfaceOzoneEGL:
  virtual intptr_t GetNativeWindow() OVERRIDE;
  virtual bool ResizeNativeWindow(const gfx::Size& viewport_size) OVERRIDE;
  virtual bool OnSwapBuffers() OVERRIDE;
  virtual scoped_ptr<gfx::VSyncProvider> CreateVSyncProvider() OVERRIDE;
  virtual bool ScheduleOverlayPlane(int plane_z_order,
                                    gfx::OverlayTransform plane_transform,
                                    scoped_refptr<ui::NativePixmap> buffer,
                                    const gfx::Rect& display_bounds,
                                    const gfx::RectF& crop_rect) OVERRIDE;

 private:
  base::WeakPtr<HardwareDisplayController> controller_;
  NativePixmapList overlay_refs_;
  std::vector<OzoneOverlayPlane> overlays_;

  DISALLOW_COPY_AND_ASSIGN(GbmSurfaceAdapter);
};

GbmSurfaceAdapter::GbmSurfaceAdapter(
    const base::WeakPtr<HardwareDisplayController>& controller)
    : controller_(controller) {}

GbmSurfaceAdapter::~GbmSurfaceAdapter() {}

intptr_t GbmSurfaceAdapter::GetNativeWindow() {
  if (!controller_)
    return 0;

  return reinterpret_cast<intptr_t>(
      static_cast<GbmSurface*>(controller_->surface())->native_surface());
}

bool GbmSurfaceAdapter::ResizeNativeWindow(const gfx::Size& viewport_size) {
  NOTIMPLEMENTED();
  return false;
}

bool GbmSurfaceAdapter::OnSwapBuffers() {
  if (!controller_)
    return false;
  bool flip_succeeded =
      controller_->SchedulePageFlip(overlays_, &overlay_refs_);
  overlays_.clear();
  if (flip_succeeded)
    controller_->WaitForPageFlipEvent();
  return flip_succeeded;
}

bool GbmSurfaceAdapter::ScheduleOverlayPlane(
    int plane_z_order,
    gfx::OverlayTransform plane_transform,
    scoped_refptr<NativePixmap> buffer,
    const gfx::Rect& display_bounds,
    const gfx::RectF& crop_rect) {
  GbmPixmap* pixmap = static_cast<GbmPixmap*>(buffer.get());
  if (!pixmap) {
    LOG(ERROR) << "ScheduleOverlayPlane passed NULL buffer";
    return false;
  }
  overlays_.push_back(OzoneOverlayPlane(pixmap->buffer(),
                                        plane_z_order,
                                        plane_transform,
                                        display_bounds,
                                        crop_rect));
  overlay_refs_.push_back(buffer);
  return true;
}

scoped_ptr<gfx::VSyncProvider> GbmSurfaceAdapter::CreateVSyncProvider() {
  return scoped_ptr<gfx::VSyncProvider>(new DriVSyncProvider(controller_));
}

}  // namespace

GbmSurfaceFactory::GbmSurfaceFactory(bool allow_surfaceless)
    : DriSurfaceFactory(NULL, NULL),
      device_(NULL),
      allow_surfaceless_(allow_surfaceless) {
}

GbmSurfaceFactory::~GbmSurfaceFactory() {}

void GbmSurfaceFactory::InitializeGpu(
    DriWrapper* dri, gbm_device* device, ScreenManager* screen_manager) {
  drm_ = dri;
  device_ = device;
  screen_manager_ = screen_manager;
}

intptr_t GbmSurfaceFactory::GetNativeDisplay() {
  CHECK(state_ == INITIALIZED);
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

scoped_ptr<ui::SurfaceOzoneEGL> GbmSurfaceFactory::CreateEGLSurfaceForWidget(
    gfx::AcceleratedWidget w) {
  CHECK(state_ == INITIALIZED);
  ResetCursor(w);

  return scoped_ptr<ui::SurfaceOzoneEGL>(
      new GbmSurfaceAdapter(screen_manager_->GetDisplayController(w)));
}

scoped_refptr<ui::NativePixmap> GbmSurfaceFactory::CreateNativePixmap(
    gfx::Size size,
    BufferFormat format) {
  scoped_refptr<GbmPixmap> buf = new GbmPixmap(device_, drm_, size);
  if (!buf->buffer()->InitializeBuffer(format, true)) {
    return NULL;
  }
  return buf;
}

bool GbmSurfaceFactory::CanShowPrimaryPlaneAsOverlay() {
  return allow_surfaceless_;
}

}  // namespace ui
