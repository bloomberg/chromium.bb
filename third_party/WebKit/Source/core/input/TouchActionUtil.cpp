// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/input/TouchActionUtil.h"

#include "core/dom/Node.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutObject.h"

namespace blink {
namespace TouchActionUtil {

TouchAction ComputeEffectiveTouchAction(const Node& node) {
  if (node.GetComputedStyle())
    return node.GetComputedStyle()->GetEffectiveTouchAction();

  return TouchAction::kTouchActionAuto;
}

}  // namespace TouchActionUtil
}  // namespace blink
