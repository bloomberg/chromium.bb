// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/gbm_surface_factory.h"

#include <gbm.h>

#include "base/files/file_path.h"
#include "third_party/khronos/EGL/egl.h"
#include "ui/ozone/platform/dri/gbm_buffer.h"
#include "ui/ozone/platform/dri/gbm_surface.h"
#include "ui/ozone/platform/dri/gbm_surfaceless.h"
#include "ui/ozone/platform/dri/screen_manager.h"
#include "ui/ozone/public/native_pixmap.h"
#include "ui/ozone/public/surface_ozone_egl.h"

namespace ui {

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
  ResetCursor(widget);

  if (allow_surfaceless_) {
    return scoped_ptr<SurfaceOzoneEGL>(
        new GbmSurfaceless(screen_manager_->GetDisplayController(widget)));
  } else {
    scoped_ptr<GbmSurface> surface(
        new GbmSurface(screen_manager_->GetDisplayController(widget),
                       device_,
                       drm_));
    if (!surface->Initialize())
      return scoped_ptr<SurfaceOzoneEGL>();

    return surface.PassAs<SurfaceOzoneEGL>();
  }
}

scoped_refptr<ui::NativePixmap> GbmSurfaceFactory::CreateNativePixmap(
    gfx::Size size,
    BufferFormat format) {
  scoped_refptr<GbmBuffer> buffer = GbmBuffer::CreateBuffer(
      drm_, device_, format, size, true);
  if (!buffer)
    return NULL;

  return scoped_refptr<GbmPixmap>(new GbmPixmap(buffer));
}

bool GbmSurfaceFactory::CanShowPrimaryPlaneAsOverlay() {
  return allow_surfaceless_;
}

}  // namespace ui
