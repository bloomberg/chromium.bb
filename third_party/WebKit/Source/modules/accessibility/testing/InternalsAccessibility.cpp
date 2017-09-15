// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/accessibility/testing/InternalsAccessibility.h"

#include "core/testing/Internals.h"
#include "modules/accessibility/AXObject.h"

namespace blink {

unsigned InternalsAccessibility::numberOfLiveAXObjects(Internals&) {
  return AXObject::NumberOfLiveAXObjects();
}

}  // namespace blink
