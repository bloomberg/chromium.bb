// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebTouchInfo_h
#define WebTouchInfo_h

#include "WebRect.h"
#include "WebTouchAction.h"

namespace blink {

struct WebTouchInfo {
  WebRect rect;
  WebTouchAction touch_action;
};

}  // namespace blink

#endif
