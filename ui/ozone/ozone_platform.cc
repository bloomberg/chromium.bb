// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "ui/base/cursor/ozone/cursor_factory_ozone.h"
#include "ui/ozone/ozone_platform.h"
#include "ui/ozone/ozone_platform_list.h"
#include "ui/ozone/ozone_switches.h"

namespace ui {

namespace {

// Helper to construct an OzonePlatform by name using the platform list.
OzonePlatform* CreatePlatform(const std::string& platform_name) {
  // Search for a matching platform in the list.
  for (int i = 0; i < kOzonePlatformCount; ++i)
    if (platform_name == kOzonePlatforms[i].name)
      return kOzonePlatforms[i].constructor();

  LOG(FATAL) << "Invalid ozone platform: " << platform_name;
  return NULL;  // not reached
}

// Returns the name of the platform to use (value of --ozone-platform flag).
std::string GetPlatformName() {
  // The first platform is the default.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kOzonePlatform) &&
      kOzonePlatformCount > 0)
    return kOzonePlatforms[0].name;
  return CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kOzonePlatform);
}

}  // namespace

OzonePlatform::OzonePlatform() {}

OzonePlatform::~OzonePlatform() {
  gfx::SurfaceFactoryOzone::SetInstance(NULL);
  ui::EventFactoryOzone::SetInstance(NULL);
  ui::CursorFactoryOzone::SetInstance(NULL);
}

// static
void OzonePlatform::Initialize() {
  if (instance_)
    return;

  std::string platform = GetPlatformName();

  TRACE_EVENT1("ozone", "OzonePlatform::Initialize", "platform", platform);

  instance_ = CreatePlatform(platform);

  // Inject ozone interfaces.
  gfx::SurfaceFactoryOzone::SetInstance(instance_->GetSurfaceFactoryOzone());
  ui::EventFactoryOzone::SetInstance(instance_->GetEventFactoryOzone());
  ui::InputMethodContextFactoryOzone::SetInstance(
      instance_->GetInputMethodContextFactoryOzone());
  ui::CursorFactoryOzone::SetInstance(instance_->GetCursorFactoryOzone());
}

// static
OzonePlatform* OzonePlatform::GetInstance() {
  CHECK(instance_) << "OzonePlatform is not initialized";
  return instance_;
}

// static
OzonePlatform* OzonePlatform::instance_;

}  // namespace ui
