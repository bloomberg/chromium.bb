// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_TEST_TEST_SURFACE_FACTORY_H_
#define UI_OZONE_PLATFORM_TEST_TEST_SURFACE_FACTORY_H_

#include "ui/ozone/public/surface_factory_ozone.h"

namespace ui {

class TestWindowManager;

class TestSurfaceFactory : public SurfaceFactoryOzone {
 public:
  TestSurfaceFactory();
  explicit TestSurfaceFactory(TestWindowManager* window_manager);
  ~TestSurfaceFactory() override;

  // SurfaceFactoryOzone:
  scoped_ptr<SurfaceOzoneCanvas> CreateCanvasForWidget(
      gfx::AcceleratedWidget w) override;
  bool LoadEGLGLES2Bindings(
      AddGLLibraryCallback add_gl_library,
      SetGLGetProcAddressProcCallback set_gl_get_proc_address) override;

 private:
  TestWindowManager* window_manager_;

  DISALLOW_COPY_AND_ASSIGN(TestSurfaceFactory);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_TEST_TEST_SURFACE_FACTORY_H_
