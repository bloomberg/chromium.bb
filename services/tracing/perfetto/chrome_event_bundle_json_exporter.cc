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
using TraceEvent = ::base::trace_event::TraceEvent;
const char* GetStringFromStringTable(
    const std::unordered_map<int, std::string>& string_table,
    int index) {
  auto it = string_table.find(index);
  DCHECK(it != string_table.end());

  return it->second.c_str();
}
void AppendProtoArrayAsJSON(std::string* out,
                            const perfetto::protos::ChromeTracedValue& array);

void AppendProtoDictAsJSON(std::string* out,
                           const perfetto::protos::ChromeTracedValue& dict);

void AppendProtoValueAsJSON(std::string* out,
                            const perfetto::protos::ChromeTracedValue& value) {
  base::trace_event::TraceEvent::TraceValue json_value;
  if (value.has_int_value()) {
    json_value.as_int = value.int_value();
    TraceEvent::AppendValueAsJSON(TRACE_VALUE_TYPE_INT, json_value, out);
  } else if (value.has_double_value()) {
    json_value.as_double = value.double_value();
    TraceEvent::AppendValueAsJSON(TRACE_VALUE_TYPE_DOUBLE, json_value, out);
  } else if (value.has_bool_value()) {
    json_value.as_bool = value.bool_value();
    TraceEvent::AppendValueAsJSON(TRACE_VALUE_TYPE_BOOL, json_value, out);
  } else if (value.has_string_value()) {
    json_value.as_string = value.string_value().c_str();
    TraceEvent::AppendValueAsJSON(TRACE_VALUE_TYPE_STRING, json_value, out);
  } else if (value.has_nested_type()) {
    if (value.nested_type() == perfetto::protos::ChromeTracedValue::ARRAY) {
      AppendProtoArrayAsJSON(out, value);
      return;
    } else if (value.nested_type() ==
               perfetto::protos::ChromeTracedValue::DICT) {
      AppendProtoDictAsJSON(out, value);
    } else {
      NOTREACHED();
    }
  } else {
    NOTREACHED();
  }
}

void AppendProtoArrayAsJSON(std::string* out,
                            const perfetto::protos::ChromeTracedValue& array) {
  out->append("[");

  bool is_first_entry = true;
  for (auto& value : array.array_values()) {
    if (!is_first_entry) {
      out->append(",");
    } else {
      is_first_entry = false;
    }

    AppendProtoValueAsJSON(out, value);
  }

  out->append("]");
}

void AppendProtoDictAsJSON(std::string* out,
                           const perfetto::protos::ChromeTracedValue& dict) {
  out->append("{");

  DCHECK_EQ(dict.dict_keys_size(), dict.dict_values_size());
  for (int i = 0; i < dict.dict_keys_size(); ++i) {
    if (i != 0) {
      out->append(",");
    }

    base::EscapeJSONString(dict.dict_keys(i), true, out);
    out->append(":");

    AppendProtoValueAsJSON(out, dict.dict_values(i));
  }

  out->append("}");
}

void OutputJSONFromArgumentValue(
    const perfetto::protos::ChromeTraceEvent::Arg& arg,
    std::string* out) {
  TraceEvent::TraceValue value;
  if (arg.has_bool_value()) {
    value.as_bool = arg.bool_value();
    TraceEvent::AppendValueAsJSON(TRACE_VALUE_TYPE_BOOL, value, out);
    return;
  }

  if (arg.has_uint_value()) {
    value.as_uint = arg.uint_value();
    TraceEvent::AppendValueAsJSON(TRACE_VALUE_TYPE_UINT, value, out);
    return;
  }

  if (arg.has_int_value()) {
    value.as_int = arg.int_value();
    TraceEvent::AppendValueAsJSON(TRACE_VALUE_TYPE_INT, value, out);
    return;
  }

  if (arg.has_double_value()) {
    value.as_double = arg.double_value();
    TraceEvent::AppendValueAsJSON(TRACE_VALUE_TYPE_DOUBLE, value, out);
    return;
  }

  if (arg.has_pointer_value()) {
    value.as_pointer = reinterpret_cast<void*>(arg.pointer_value());
    TraceEvent::AppendValueAsJSON(TRACE_VALUE_TYPE_POINTER, value, out);
    return;
  }

  if (arg.has_string_value()) {
    std::string str = arg.string_value();
    value.as_string = &str[0];
    TraceEvent::AppendValueAsJSON(TRACE_VALUE_TYPE_STRING, value, out);
    return;
  }

  if (arg.has_json_value()) {
    *out += arg.json_value();
    return;
  }

  if (arg.has_traced_value()) {
    AppendProtoDictAsJSON(out, arg.traced_value());
    return;
  }

  NOTREACHED();
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
      OutputJSONFromArgumentValue(arg, maybe_arg->mutable_out());
    }
  }
  // Do not add anything to |trace_event_builder| unless you destroy
  // |args_builder|.
}
}  // namespace tracing
