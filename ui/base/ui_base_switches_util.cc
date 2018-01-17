// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ui_base_switches_util.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "ui/base/ui_base_switches.h"

namespace switches {

bool IsTouchDragDropEnabled() {
#if defined(OS_CHROMEOS) || defined(OS_ANDROID)
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableTouchDragDrop);
#else
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableTouchDragDrop);
#endif
}

bool IsMusHostingViz() {
#if defined(USE_AURA)
  auto* cmd = base::CommandLine::ForCurrentProcess();
  return cmd->HasSwitch(switches::kMus) &&
         cmd->HasSwitch(switches::kMusHostingViz);
#else
  return false;
#endif
}

}  // namespace switches
