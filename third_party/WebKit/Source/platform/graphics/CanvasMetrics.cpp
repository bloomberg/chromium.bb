// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/CanvasMetrics.h"

#include "public/platform/Platform.h"

namespace blink {

void CanvasMetrics::countCanvasContextUsage(const CanvasContextUsage canvasContextUsage)
{
    Platform::current()->histogramEnumeration("WebCore.CanvasContextUsage", canvasContextUsage, CanvasContextUsage::NumberOfUsages);
}

} // namespace blink
