// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ozone/surface_factory_ozone.h"

#include <stdlib.h>

namespace ui {

// static
SurfaceFactoryOzone* SurfaceFactoryOzone::impl_ = NULL;

class SurfaceFactoryOzoneStub : public SurfaceFactoryOzone {
 public:
  SurfaceFactoryOzoneStub() {}
  virtual ~SurfaceFactoryOzoneStub() {}

  virtual void InitializeHardware() OVERRIDE {}
  virtual void ShutdownHardware() OVERRIDE {}
  virtual gfx::AcceleratedWidget GetAcceleratedWidget() OVERRIDE { return 0; }
  virtual gfx::AcceleratedWidget RealizeAcceleratedWidget(
      gfx::AcceleratedWidget w) OVERRIDE {
    return 0;
  }
  virtual bool LoadEGLGLES2Bindings() OVERRIDE { return true; }
  virtual bool AttemptToResizeAcceleratedWidget(
      gfx::AcceleratedWidget w,
      const gfx::Rect& bounds) OVERRIDE {
    return false;
  }
  virtual gfx::VSyncProvider* GetVSyncProvider(
      gfx::AcceleratedWidget w) OVERRIDE {
    return NULL;
  }
};

SurfaceFactoryOzone::SurfaceFactoryOzone() {
}

SurfaceFactoryOzone::~SurfaceFactoryOzone() {
}

SurfaceFactoryOzone* SurfaceFactoryOzone::GetInstance() {
  CHECK(impl_) << "SurfaceFactoryOzone accessed before constructed";
  return impl_;
}

void SurfaceFactoryOzone::SetInstance(SurfaceFactoryOzone* impl) {
  impl_ = impl;
}

const char* SurfaceFactoryOzone::DefaultDisplaySpec() {
  char* envvar = getenv("ASH_DISPLAY_SPEC");
  if (envvar)
    return envvar;
  return  "720x1280*2";
}

// static
SurfaceFactoryOzone* SurfaceFactoryOzone::CreateTestHelper() {
  return new SurfaceFactoryOzoneStub;
}

}  // namespace ui
