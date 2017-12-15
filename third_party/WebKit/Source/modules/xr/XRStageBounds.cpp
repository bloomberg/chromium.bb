// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/xr/XRStageBounds.h"

namespace blink {

void XRStageBounds::Trace(blink::Visitor* visitor) {
  visitor->Trace(geometry_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
