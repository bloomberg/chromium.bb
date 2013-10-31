// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/logging.h"
#include "ui/ozone/ozone_platform.h"

namespace ui {

OzonePlatform::OzonePlatform() {}

OzonePlatform::~OzonePlatform() {
  gfx::SurfaceFactoryOzone::SetInstance(NULL);
  ui::EventFactoryOzone::SetInstance(NULL);
}

// static
void OzonePlatform::Initialize() {
  if (instance_)
    return;

  instance_ = CreateDefaultOzonePlatform();

  // Inject ozone interfaces.
  gfx::SurfaceFactoryOzone::SetInstance(instance_->GetSurfaceFactoryOzone());
  ui::EventFactoryOzone::SetInstance(instance_->GetEventFactoryOzone());
}

// static
OzonePlatform* OzonePlatform::instance_;

}  // namespace ui
