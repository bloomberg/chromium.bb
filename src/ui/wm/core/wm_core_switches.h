// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_WM_CORE_SWITCHES_H_
#define UI_WM_CORE_WM_CORE_SWITCHES_H_

#include "build/build_config.h"
#include "ui/wm/core/wm_core_export.h"

namespace wm {
namespace switches {

// Note: If you add a switch, consider if it needs to be copied to a subsequent
// command line if the process executes a new copy of itself.  (For example,
// see chromeos::LoginUtil::GetOffTheRecordCommandLine().)

// Please keep alphabetized.
WM_CORE_EXPORT extern const char kWindowAnimationsDisabled[];

}  // namespace switches
}  // namespace wm

#endif  // UI_WM_CORE_WM_CORE_SWITCHES_H_
