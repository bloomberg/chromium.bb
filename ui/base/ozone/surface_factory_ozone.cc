// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ozone/surface_factory_ozone.h"

#include <stdlib.h>

namespace ui {

// static
SurfaceFactoryOzone* SurfaceFactoryOzone::impl_ = NULL;

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

}  // namespace ui
