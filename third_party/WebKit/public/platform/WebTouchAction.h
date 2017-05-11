// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebTouchAction_h
#define WebTouchAction_h

#include "cc/input/touch_action.h"

namespace blink {

// The only reason of creating a WebTouchAction is that there is a virtual
// function under WebWidgetClient that is overridden by classes under content/
// and WebKit/.
using WebTouchAction = cc::TouchAction;

}  // namespace blink

#endif  // WebTouchAction_h
