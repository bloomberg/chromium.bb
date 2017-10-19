// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/vr/VRDisplayCapabilities.h"

namespace blink {

VRDisplayCapabilities::VRDisplayCapabilities()
    : has_position_(false), has_external_display_(false), can_present_(false) {}

void VRDisplayCapabilities::Trace(blink::Visitor* visitor) {}

}  // namespace blink
