// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/timing/PerformanceServerTiming.h"

#include "bindings/core/v8/V8ObjectBuilder.h"
#include "platform/loader/fetch/ResourceTimingInfo.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

PerformanceServerTiming::PerformanceServerTiming(const String& name,
                                                 double duration,
                                                 const String& description)
    : name_(name), duration_(duration), description_(description) {}

PerformanceServerTiming::~PerformanceServerTiming() = default;

ScriptValue PerformanceServerTiming::toJSONForBinding(
    ScriptState* script_state) const {
  V8ObjectBuilder builder(script_state);
  builder.AddString("name", name());
  builder.AddNumber("duration", duration());
  builder.AddString("description", description());
  return builder.GetScriptValue();
}

WebVector<WebServerTimingInfo> PerformanceServerTiming::ParseServerTiming(
    const ResourceTimingInfo& info,
    ShouldAllowTimingDetails should_allow_timing_details) {
  WebVector<WebServerTimingInfo> result;
  if (RuntimeEnabledFeatures::ServerTimingEnabled()) {
    const ResourceResponse& response = info.FinalResponse();
    std::unique_ptr<ServerTimingHeaderVector> headers = ParseServerTimingHeader(
        response.HttpHeaderField(HTTPNames::Server_Timing));
    result.reserve(headers->size());
    for (const auto& header : *headers) {
      if (should_allow_timing_details == ShouldAllowTimingDetails::Yes) {
        result.emplace_back(header->Name(), header->Duration(),
                            header->Description());
      } else {
        result.emplace_back(header->Name(), 0.0, String());
      }
    }
  }
  return result;
}

HeapVector<Member<PerformanceServerTiming>>
PerformanceServerTiming::FromParsedServerTiming(
    const WebVector<WebServerTimingInfo>& entries) {
  HeapVector<Member<PerformanceServerTiming>> result;
  for (const auto& entry : entries) {
    result.push_back(new PerformanceServerTiming(entry.name, entry.duration,
                                                 entry.description));
  }
  return result;
}

}  // namespace blink
