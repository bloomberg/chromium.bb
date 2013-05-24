// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ozone/surface_factory_ozone.h"

#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"

namespace ui {

// Implementation.
static base::LazyInstance<scoped_ptr<SurfaceFactoryOzone> > impl_ =
    LAZY_INSTANCE_INITIALIZER;

SurfaceFactoryOzone::SurfaceFactoryOzone() {
}

SurfaceFactoryOzone::~SurfaceFactoryOzone() {
}

SurfaceFactoryOzone* SurfaceFactoryOzone::GetInstance() {
  return impl_.Get().get();
}

void SurfaceFactoryOzone::SetInstance(SurfaceFactoryOzone* impl) {
  impl_.Get().reset(impl);
}

const char* SurfaceFactoryOzone::DefaultDisplaySpec() {
  char* envvar = getenv("ASH_DISPLAY_SPEC");
  if (envvar)
    return envvar;
  return  "720x1280*2";
}

}  // namespace ui
