// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TOUCHUI_TOUCH_MODE_SUPPORT_H_
#define UI_VIEWS_TOUCHUI_TOUCH_MODE_SUPPORT_H_
#pragma once

#include "base/basictypes.h"
#include "ui/views/views_export.h"

class VIEWS_EXPORT TouchModeSupport {
 public:
  // Returns whether we should be operating in touch-friendly mode.
  static bool IsTouchOptimized();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(TouchModeSupport);
};

#endif  // UI_VIEWS_TOUCHUI_TOUCH_MODE_SUPPORT_H_
