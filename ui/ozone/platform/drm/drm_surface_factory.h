// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_DRM_SURFACE_FACTORY_H_
#define UI_OZONE_PLATFORM_DRM_DRM_SURFACE_FACTORY_H_

#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace ui {

class ScreenManager;
class SurfaceOzoneCanvas;

// SurfaceFactoryOzone implementation on top of DRM/KMS using dumb buffers.
// This implementation is used in conjunction with the software rendering
// path.
class DrmSurfaceFactory : public SurfaceFactoryOzone {
 public:
  DrmSurfaceFactory(ScreenManager* screen_manager);
  ~DrmSurfaceFactory() override;

  // SurfaceFactoryOzone:
  scoped_ptr<SurfaceOzoneCanvas> CreateCanvasForWidget(
      gfx::AcceleratedWidget widget) override;
  bool LoadEGLGLES2Bindings(
      AddGLLibraryCallback add_gl_library,
      SetGLGetProcAddressProcCallback set_gl_get_proc_address) override;

 protected:
  ScreenManager* screen_manager_;  // Not owned.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(DrmSurfaceFactory);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_DRM_SURFACE_FACTORY_H_
