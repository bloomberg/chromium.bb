// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_CACA_CACA_WINDOW_MANAGER_H_
#define UI_OZONE_PLATFORM_CACA_CACA_WINDOW_MANAGER_H_

#include "base/id_map.h"
#include "base/memory/scoped_ptr.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace gfx {
class Rect;
}

namespace ui {

class CacaWindow;

class CacaWindowManager : public SurfaceFactoryOzone {
 public:
  CacaWindowManager();
  virtual ~CacaWindowManager();

  // Register a new libcaca window/instance. Returns the window id.
  int32_t AddWindow(CacaWindow* window);

  // Remove a libcaca window/instance.
  void RemoveWindow(int32_t window_id, CacaWindow* window);

  // ui::SurfaceFactoryOzone overrides:
  virtual bool LoadEGLGLES2Bindings(
      AddGLLibraryCallback add_gl_library,
      SetGLGetProcAddressProcCallback set_gl_get_proc_address) OVERRIDE;
  virtual scoped_ptr<SurfaceOzoneCanvas> CreateCanvasForWidget(
      gfx::AcceleratedWidget widget) OVERRIDE;

 private:
  IDMap<CacaWindow> windows_;

  DISALLOW_COPY_AND_ASSIGN(CacaWindowManager);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_CACA_CACA_WINDOW_MANAGER_H_
