// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/canvas/CanvasContextCreationAttributesCore.h"

namespace blink {

CanvasContextCreationAttributesCore::CanvasContextCreationAttributesCore() {}

CanvasContextCreationAttributesCore::CanvasContextCreationAttributesCore(
    blink::CanvasContextCreationAttributesCore const& attrs) = default;

CanvasContextCreationAttributesCore::~CanvasContextCreationAttributesCore() {}

void CanvasContextCreationAttributesCore::Trace(blink::Visitor* visitor) {
  visitor->Trace(compatible_xr_device);
}

}  // namespace blink
