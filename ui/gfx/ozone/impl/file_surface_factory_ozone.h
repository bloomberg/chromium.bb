// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_OZONE_IMPL_FILE_SURFACE_FACTORY_OZONE_H_
#define UI_GFX_OZONE_IMPL_FILE_SURFACE_FACTORY_OZONE_H_

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/ozone/surface_factory_ozone.h"
#include "ui/gfx/skia_util.h"

class SkBitmapDevice;
class SkCanvas;

namespace gfx {

class FileSurfaceFactoryOzone : public SurfaceFactoryOzone {
 public:
  explicit FileSurfaceFactoryOzone(const base::FilePath& dump_location);
  virtual ~FileSurfaceFactoryOzone();

 private:
  // SurfaceFactoryOzone:
  virtual HardwareState InitializeHardware() OVERRIDE;
  virtual void ShutdownHardware() OVERRIDE;
  virtual AcceleratedWidget GetAcceleratedWidget() OVERRIDE;
  virtual AcceleratedWidget RealizeAcceleratedWidget(
      AcceleratedWidget widget) OVERRIDE;
  virtual bool LoadEGLGLES2Bindings(
      AddGLLibraryCallback add_gl_library,
      SetGLGetProcAddressProcCallback set_gl_get_proc_address) OVERRIDE;
  virtual bool AttemptToResizeAcceleratedWidget(AcceleratedWidget widget,
                                                const Rect& bounds) OVERRIDE;
  virtual bool SchedulePageFlip(AcceleratedWidget widget) OVERRIDE;
  virtual SkCanvas* GetCanvasForWidget(AcceleratedWidget widget) OVERRIDE;
  virtual VSyncProvider* GetVSyncProvider(AcceleratedWidget widget) OVERRIDE;

  base::FilePath location_;
  skia::RefPtr<SkBitmapDevice> device_;
  skia::RefPtr<SkCanvas> canvas_;

  DISALLOW_COPY_AND_ASSIGN(FileSurfaceFactoryOzone);
};

}  // namespace gfx

#endif  // UI_GFX_OZONE_IMPL_FILE_SURFACE_FACTORY_OZONE_H_
