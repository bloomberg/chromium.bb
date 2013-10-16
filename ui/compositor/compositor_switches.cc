// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/compositor_switches.h"

#include "base/command_line.h"

namespace switches {

const char kDisableTestCompositor[] = "disable-test-compositor";

const char kUIDisableDeadlineScheduling[] = "ui-disable-deadline-scheduling";

const char kUIDisableThreadedCompositing[] = "ui-disable-threaded-compositing";

const char kUIEnableDeadlineScheduling[] = "ui-enable-deadline-scheduling";

const char kUIEnableSoftwareCompositing[] = "ui-enable-software-compositing";

const char kUIEnableThreadedCompositing[] = "ui-enable-threaded-compositing";

const char kUIMaxFramesPending[] = "ui-max-frames-pending";

const char kUIShowPaintRects[] = "ui-show-paint-rects";

bool IsUIDeadlineSchedulingEnabled() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  // Default to disabled.
  bool enabled = false;

  // Default to enabled for Aura.
  enabled = true;

  // Flags override.
  enabled |= command_line.HasSwitch(switches::kUIEnableDeadlineScheduling);
  enabled &= !command_line.HasSwitch(switches::kUIDisableDeadlineScheduling);

  return enabled;
}

}  // namespace switches
