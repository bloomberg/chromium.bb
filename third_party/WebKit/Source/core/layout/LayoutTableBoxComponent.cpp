// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutTableBoxComponent.h"

#include "core/style/ComputedStyle.h"

namespace blink {

void LayoutTableBoxComponent::imageChanged(WrappedImagePtr image, const IntRect* rect)
{
    setShouldDoFullPaintInvalidation();
}

} // namespace blink
