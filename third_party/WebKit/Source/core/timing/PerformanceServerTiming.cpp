// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/timing/PerformanceServerTiming.h"

#include "bindings/core/v8/V8ObjectBuilder.h"
#include "core/timing/PerformanceBase.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

PerformanceServerTiming::PerformanceServerTiming(const String& name,
                                                 const String& metric,
                                                 double duration,
                                                 const String& description)
    : PerformanceEntry(name, "server", 0.0, duration),
      metric_(metric),
      description_(description) {}

PerformanceServerTiming::~PerformanceServerTiming() {}

String PerformanceServerTiming::metric() const {
  return metric_;
}

String PerformanceServerTiming::description() const {
  return description_;
}

void PerformanceServerTiming::BuildJSONValue(V8ObjectBuilder& builder) const {
  PerformanceEntry::BuildJSONValue(builder);
  builder.AddString("metric", metric());
  builder.AddString("description", description());
}

}  // namespace blink
