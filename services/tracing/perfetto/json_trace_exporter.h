// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PERFETTO_JSON_TRACE_EXPORTER_H_
#define SERVICES_TRACING_PERFETTO_JSON_TRACE_EXPORTER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/values.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_packet.h"

namespace perfetto {
namespace protos {
class ChromeTracedValue;
}  // namespace protos
}  // namespace perfetto

namespace tracing {

// Serializes the supplied proto into JSON, using the same
// format as TracedValue::AppendAsTraceFormat.
void AppendProtoDictAsJSON(std::string* out,
                           const perfetto::protos::ChromeTracedValue& dict);

// Converts proto-encoded trace data into the legacy JSON trace format.
// Conversion happens on-the-fly as new trace packets are received.
class JSONTraceExporter {
 public:
  using OnTraceEventJSONCallback =
      base::RepeatingCallback<void(const std::string& json,
                                   base::DictionaryValue* metadata,
                                   bool has_more)>;

  JSONTraceExporter(OnTraceEventJSONCallback callback);
  virtual ~JSONTraceExporter();

  // Called to notify the exporter of new trace packets. Will call the
  // |json_callback| passed in the constructor with the converted trace data.
  void OnTraceData(std::vector<perfetto::TracePacket> packets, bool has_more);

 private:
  OnTraceEventJSONCallback json_callback_;
  bool has_output_json_preamble_ = false;
  bool has_output_first_event_ = false;
  std::unique_ptr<base::DictionaryValue> metadata_;
  std::string legacy_system_ftrace_output_;
  std::string legacy_system_trace_events_;

  DISALLOW_COPY_AND_ASSIGN(JSONTraceExporter);
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PERFETTO_JSON_TRACE_EXPORTER_H_
