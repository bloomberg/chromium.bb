// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/test/ozone_platform_test.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "ui/ozone/ozone_platform.h"
#include "ui/ozone/ozone_switches.h"

namespace ui {

OzonePlatformTest::OzonePlatformTest(const base::FilePath& dump_file)
    : surface_factory_ozone_(dump_file) {}

OzonePlatformTest::~OzonePlatformTest() {}

gfx::SurfaceFactoryOzone* OzonePlatformTest::GetSurfaceFactoryOzone() {
  return &surface_factory_ozone_;
}

ui::EventFactoryOzone* OzonePlatformTest::GetEventFactoryOzone() {
  return &event_factory_ozone_;
}

ui::InputMethodContextFactoryOzone*
OzonePlatformTest::GetInputMethodContextFactoryOzone() {
  return &input_method_context_factory_ozone_;
}

ui::CursorFactoryOzone* OzonePlatformTest::GetCursorFactoryOzone() {
  return &cursor_factory_ozone_;
}

OzonePlatform* CreateOzonePlatformTest() {
  CommandLine* cmd = CommandLine::ForCurrentProcess();
  base::FilePath location = base::FilePath("/dev/null");
  if (cmd->HasSwitch(switches::kOzoneDumpFile))
    location = cmd->GetSwitchValuePath(switches::kOzoneDumpFile);
  return new OzonePlatformTest(location);
}

}  // namespace ui
