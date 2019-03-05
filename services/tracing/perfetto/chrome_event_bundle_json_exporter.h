// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PERFETTO_CHROME_EVENT_BUNDLE_JSON_EXPORTER_H_
#define SERVICES_TRACING_PERFETTO_CHROME_EVENT_BUNDLE_JSON_EXPORTER_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "base/macros.h"
#include "services/tracing/perfetto/json_trace_exporter.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_packet.h"

namespace perfetto {
namespace protos {
class ChromeTraceEvent;
}
}  // namespace perfetto

namespace tracing {

// Converts proto-encoded ChromeEventBundles into the legacy JSON trace format.
// Conversion happens on-the-fly as new trace packets are received.
class ChromeEventBundleJsonExporter : public JSONTraceExporter {
 public:
  ChromeEventBundleJsonExporter(
      ArgumentFilterPredicate argument_filter_predicate,
      OnTraceEventJSONCallback callback);
  ~ChromeEventBundleJsonExporter() override = default;

 protected:
  // Assumes each packet can be decoded as a ChromeTracePacket. Only uses
  // packets that have either |trace_stats| or |chrome_events| fields.
  void ProcessPackets(
      const std::vector<perfetto::TracePacket>& packets) override;

 private:
  // Takes |event| and constructs the output json through use of the
  // |trace_event_builder|.
  void ConstructTraceEventJSONWithBuilder(
      const perfetto::protos::ChromeTraceEvent& event,
      const std::unordered_map<int, std::string>& string_table,
      ScopedJSONTraceEventAppender&& trace_event_builder);

  DISALLOW_COPY_AND_ASSIGN(ChromeEventBundleJsonExporter);
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PERFETTO_CHROME_EVENT_BUNDLE_JSON_EXPORTER_H_
