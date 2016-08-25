// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/timing/PerformanceLongTaskTiming.h"

namespace blink {

PerformanceLongTaskTiming::PerformanceLongTaskTiming(
    double startTime, double endTime, String frameContextUrl)
    : PerformanceEntry(frameContextUrl, "longtask", startTime, endTime)
{
}

PerformanceLongTaskTiming::~PerformanceLongTaskTiming()
{
}

DEFINE_TRACE(PerformanceLongTaskTiming)
{
    PerformanceEntry::trace(visitor);
}

} // namespace blink
