// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/CanvasMetrics.h"

#include "platform/Histogram.h"
#include "platform/wtf/StdLibExtras.h"
#include "platform/wtf/Threading.h"

namespace blink {

void CanvasMetrics::CountCanvasContextUsage(
    const CanvasContextUsage canvas_context_usage) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, usage_histogram,
      ("WebCore.CanvasContextUsage", CanvasContextUsage::kNumberOfUsages));
  usage_histogram.Count(canvas_context_usage);
}

}  // namespace blink
