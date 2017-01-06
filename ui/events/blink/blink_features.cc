// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/blink_features.h"

namespace features {

// Enables VSync aligned input for GestureScroll/Pinch on compositor thread.
// Tracking: https://crbug.com/625689
const base::Feature kVsyncAlignedInputEvents{"VsyncAlignedInput",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSendMouseLeaveEvents {
  "SendMouseLeaveEvents",
// TODO(chaopeng) this fix only for chromeos now, should convert ET_MOUSE_EXITED
// to MouseLeave when crbug.com/450631 fixed.
#if defined(OS_CHROMEOS)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};
}
