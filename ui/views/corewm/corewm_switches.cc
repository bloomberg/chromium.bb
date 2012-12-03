// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/corewm/corewm_switches.h"

namespace views {
namespace corewm {
namespace switches {

// When set uses the CoreWM FocusController in place of the Ash
// ActivationController and aura FocusManager.
const char kUseFocusController[] = "views-corewm-use-focus-controller";

// If present animations are disabled.
const char kWindowAnimationsDisabled[] =
    "views-corewm-window-animations-disabled";

}  // namespace switches
}  // namespace corewm
}  // namespace views
