// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/vr/latest/VRStageBounds.h"

namespace blink {

void VRStageBounds::Trace(blink::Visitor* visitor) {
  visitor->Trace(geometry_);
}

}  // namespace blink
