// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/timing/PerformancePaintTiming.h"

#include "bindings/core/v8/V8ObjectBuilder.h"

namespace blink {

PerformancePaintTiming::PerformancePaintTiming(PaintType type, double startTime)
    : PerformanceEntry(fromPaintTypeToString(type),
                       "paint",
                       startTime,
                       startTime) {}

PerformancePaintTiming::~PerformancePaintTiming() {}

String PerformancePaintTiming::fromPaintTypeToString(PaintType type) {
  switch (type) {
    case PaintType::FirstPaint:
      return "first-paint";
    case PaintType::FirstContentfulPaint:
      return "first-contentful-paint";
  }
  NOTREACHED();
  return "";
}
}  // namespace blink
