// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/timing/PerformanceServerTiming.h"

#include "bindings/core/v8/V8ObjectBuilder.h"
#include "platform/loader/fetch/ResourceTimingInfo.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

PerformanceServerTiming::PerformanceServerTiming(
    const String& metric,
    double value,
    const String& description,
    ShouldAllowTimingDetails shouldAllowTimingDetails)
    : metric_(metric),
      value_(value),
      description_(description),
      shouldAllowTimingDetails_(shouldAllowTimingDetails) {}

PerformanceServerTiming::~PerformanceServerTiming() {}

String PerformanceServerTiming::metric() const {
  return metric_;
}

double PerformanceServerTiming::value() const {
  return shouldAllowTimingDetails_ == ShouldAllowTimingDetails::Yes ? value_
                                                                    : 0.0;
}

String PerformanceServerTiming::description() const {
  return shouldAllowTimingDetails_ == ShouldAllowTimingDetails::Yes
             ? description_
             : "";
}

ScriptValue PerformanceServerTiming::toJSONForBinding(
    ScriptState* script_state) const {
  V8ObjectBuilder builder(script_state);
  builder.AddString("metric", metric());
  builder.AddNumber("value", value());
  builder.AddString("description", description());
  return builder.GetScriptValue();
}

PerformanceServerTimingVector PerformanceServerTiming::ParseServerTiming(
    const ResourceTimingInfo& info,
    ShouldAllowTimingDetails shouldAllowTimingDetails) {
  PerformanceServerTimingVector entries;
  if (RuntimeEnabledFeatures::ServerTimingEnabled()) {
    const ResourceResponse& response = info.FinalResponse();
    std::unique_ptr<ServerTimingHeaderVector> headers = ParseServerTimingHeader(
        response.HttpHeaderField(HTTPNames::Server_Timing));
    for (const auto& header : *headers) {
      entries.push_back(new PerformanceServerTiming(
          header->metric, header->value, header->description,
          shouldAllowTimingDetails));
    }
  }
  return entries;
}

}  // namespace blink
