// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WIN_LOCK_STATE_H_
#define UI_BASE_WIN_LOCK_STATE_H_

#include "ui/base/ui_base_export.h"

namespace ui {

// Returns true if the screen is currently locked.
UI_BASE_EXPORT bool IsWorkstationLocked();

}  // namespace ui

#endif  // UI_BASE_WIN_LOCK_STATE_H_
