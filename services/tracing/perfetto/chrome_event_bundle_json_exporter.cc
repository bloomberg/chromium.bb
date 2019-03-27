// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/perfetto/chrome_event_bundle_json_exporter.h"

#include <utility>

#include "base/json/string_escape.h"
#include "base/trace_event/trace_event.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_trace_event.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_trace_packet.pb.h"

namespace tracing {
namespace {
const char* GetStringFromStringTable(
    const std::unordered_map<int, std::string>& string_table,
    int index) {
  auto it = string_table.find(index);
  DCHECK(it != string_table.end());

  return it->second.c_str();
}

}  // namespace

ChromeEventBundleJsonExporter::ChromeEventBundleJsonExporter(
    JSONTraceExporter::ArgumentFilterPredicate argument_filter_predicate,
    JSONTraceExporter::OnTraceEventJSONCallback callback)
    : JSONTraceExporter(std::move(argument_filter_predicate),
                        std::move(callback)) {}

void ChromeEventBundleJsonExporter::ProcessPackets(
    const std::vector<perfetto::TracePacket>& packets) {
  for (auto& encoded_packet : packets) {
    perfetto::protos::ChromeTracePacket packet;
    bool decoded = encoded_packet.Decode(&packet);
    DCHECK(decoded);

    if (packet.has_trace_stats()) {
      SetTraceStatsMetadata(packet.trace_stats());
      continue;
    }

    if (!packet.has_chrome_events()) {
      continue;
    }

    auto& bundle = packet.chrome_events();

    if (ShouldOutputTraceEvents()) {
      std::unordered_map<int, std::string> string_table;
      for (auto& string_table_entry : bundle.string_table()) {
        string_table[string_table_entry.index()] = string_table_entry.value();
      }

      for (auto& event : bundle.trace_events()) {
        const char* name =
            event.has_name_index()
                ? GetStringFromStringTable(string_table, event.name_index())
                : event.name().c_str();
        const char* category_group_name =
            event.has_category_group_name_index()
                ? GetStringFromStringTable(string_table,
                                           event.category_group_name_index())
                : event.category_group_name().c_str();

        ConstructTraceEventJSONWithBuilder(
            event, string_table,
            AddTraceEvent(name, category_group_name, event.phase(),
                          event.timestamp(), event.process_id(),
                          event.thread_id()));
      }
    }

    for (auto& metadata : bundle.metadata()) {
      AddChromeMetadata(metadata);
    }

    for (auto& legacy_ftrace_output : bundle.legacy_ftrace_output()) {
      AddLegacyFtrace(legacy_ftrace_output);
    }

    for (auto& legacy_json_trace : bundle.legacy_json_trace()) {
      AddChromeLegacyJSONTrace(legacy_json_trace);
    }
  }
}

void ChromeEventBundleJsonExporter::ConstructTraceEventJSONWithBuilder(
    const perfetto::protos::ChromeTraceEvent& event,
    const std::unordered_map<int, std::string>& string_table,
    JSONTraceExporter::ScopedJSONTraceEventAppender&& trace_event_builder) {
  if (event.has_duration()) {
    trace_event_builder.AddDuration(event.duration());
  }

  if (event.has_thread_duration()) {
    trace_event_builder.AddThreadDuration(event.thread_duration());
  }

  if (event.has_thread_timestamp()) {
    trace_event_builder.AddThreadTimestamp(event.thread_timestamp());
  }

  DCHECK((event.has_scope() && event.scope() != "") || !event.has_scope());
  trace_event_builder.AddFlags(event.flags(),
                               event.has_id()
                                   ? base::Optional<uint64_t>(event.id())
                                   : base::Optional<uint64_t>(),
                               event.scope());

  if (event.has_bind_id()) {
    trace_event_builder.AddBindId(event.bind_id());
  }
  auto args_builder = trace_event_builder.BuildArgs();
  for (const auto& arg : event.args()) {
    const std::string& arg_name =
        arg.has_name_index()
            ? GetStringFromStringTable(string_table, arg.name_index())
            : arg.name();

    auto* maybe_arg = args_builder->MaybeAddArg(arg_name);
    if (maybe_arg) {
      OutputJSONFromArgumentProto(arg, maybe_arg->mutable_out());
    }
  }
  // Do not add anything to |trace_event_builder| unless you destroy
  // |args_builder|.
}
}  // namespace tracing
