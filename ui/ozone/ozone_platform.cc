// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/logging.h"
#include "ui/ozone/ozone_platform.h"
#include "ui/ozone/ozone_platform_list.h"
#include "ui/ozone/ozone_switches.h"

namespace ui {

namespace {

// Helper to construct an OzonePlatform by name using the platform list.
OzonePlatform* CreatePlatform(const std::string& platform_name) {
  // The first platform is the defualt.
  if (platform_name == "default" && kOzonePlatformCount > 0)
    return kOzonePlatforms[0].constructor();

  // Otherwise, search for a matching platform in the list.
  for (int i = 0; i < kOzonePlatformCount; ++i)
    if (platform_name == kOzonePlatforms[i].name)
      return kOzonePlatforms[i].constructor();

  LOG(FATAL) << "Invalid ozone platform: " << platform_name;
  return NULL;  // not reached
}

// Returns the name of the platform to use (value of --ozone-platform flag).
std::string GetRequestedPlatform() {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kOzonePlatform))
    return "default";
  return CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kOzonePlatform);
}

}  // namespace

OzonePlatform::OzonePlatform() {}

OzonePlatform::~OzonePlatform() {
  gfx::SurfaceFactoryOzone::SetInstance(NULL);
  ui::EventFactoryOzone::SetInstance(NULL);
}

// static
void OzonePlatform::Initialize() {
  if (instance_)
    return;

  instance_ = CreatePlatform(GetRequestedPlatform());

  // Inject ozone interfaces.
  gfx::SurfaceFactoryOzone::SetInstance(instance_->GetSurfaceFactoryOzone());
  ui::EventFactoryOzone::SetInstance(instance_->GetEventFactoryOzone());
}

// static
OzonePlatform* OzonePlatform::instance_;

}  // namespace ui
