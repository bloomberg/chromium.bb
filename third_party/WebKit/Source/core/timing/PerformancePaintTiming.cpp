// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/timing/PerformancePaintTiming.h"

#include "bindings/core/v8/V8ObjectBuilder.h"

namespace blink {

PerformancePaintTiming::PerformancePaintTiming(PaintType type,
                                               double start_time)
    : PerformanceEntry(FromPaintTypeToString(type),
                       "paint",
                       start_time,
                       start_time) {}

PerformancePaintTiming::~PerformancePaintTiming() = default;

String PerformancePaintTiming::FromPaintTypeToString(PaintType type) {
  switch (type) {
    case PaintType::kFirstPaint:
      return "first-paint";
    case PaintType::kFirstContentfulPaint:
      return "first-contentful-paint";
  }
  NOTREACHED();
  return "";
}
}  // namespace blink
