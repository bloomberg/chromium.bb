// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TouchAction_h
#define TouchAction_h

#include "cc/input/touch_action.h"

namespace blink {

const size_t kTouchActionBits = cc::kTouchActionBits;
using TouchAction = cc::TouchAction;

}  // namespace blink

#endif  // TouchAction_h
