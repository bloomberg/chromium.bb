// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TouchActionUtil_h
#define TouchActionUtil_h

#include "core/CoreExport.h"
#include "platform/graphics/TouchAction.h"

namespace blink {

class Node;

namespace TouchActionUtil {
CORE_EXPORT TouchAction ComputeEffectiveTouchAction(const Node&);
}  // namespace TouchActionUtil

}  // namespace blink

#endif  // TouchActionUtil_h
