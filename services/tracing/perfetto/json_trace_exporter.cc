// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/perfetto/json_trace_exporter.h"

#include <unordered_map>
#include <utility>

#include "base/format_macros.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_packet.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_trace_event.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_trace_packet.pb.h"

using TraceEvent = base::trace_event::TraceEvent;

namespace tracing {

namespace {

const size_t kTraceEventBufferSizeInBytes = 100 * 1024;

void AppendProtoArrayAsJSON(std::string* out,
                            const perfetto::protos::ChromeTracedValue& array);

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

const char* GetStringFromStringTable(
    const std::unordered_map<int, std::string>& string_table,
    int index) {
  auto it = string_table.find(index);
  DCHECK(it != string_table.end());

  return it->second.c_str();
}

void OutputJSONFromTraceEventProto(
    const perfetto::protos::ChromeTraceEvent& event,
    std::string* out,
    const std::unordered_map<int, std::string>& string_table) {
  char phase = static_cast<char>(event.phase());
  const char* name =
      event.has_name_index()
          ? GetStringFromStringTable(string_table, event.name_index())
          : event.name().c_str();
  const char* category_group_name =
      event.has_category_group_name_index()
          ? GetStringFromStringTable(string_table,
                                     event.category_group_name_index())
          : event.category_group_name().c_str();

  base::StringAppendF(out,
                      "{\"pid\":%i,\"tid\":%i,\"ts\":%" PRId64
                      ",\"ph\":\"%c\",\"cat\":\"%s\",\"name\":",
                      event.process_id(), event.thread_id(), event.timestamp(),
                      phase, category_group_name);
  base::EscapeJSONString(name, true, out);

  if (event.has_duration()) {
    base::StringAppendF(out, ",\"dur\":%" PRId64, event.duration());
  }

  if (event.has_thread_duration()) {
    base::StringAppendF(out, ",\"tdur\":%" PRId64, event.thread_duration());
  }

  if (event.has_thread_timestamp()) {
    base::StringAppendF(out, ",\"tts\":%" PRId64, event.thread_timestamp());
  }

  // Output async tts marker field if flag is set.
  if (event.flags() & TRACE_EVENT_FLAG_ASYNC_TTS) {
    base::StringAppendF(out, ", \"use_async_tts\":1");
  }

  // If id_ is set, print it out as a hex string so we don't loose any
  // bits (it might be a 64-bit pointer).
  unsigned int id_flags =
      event.flags() & (TRACE_EVENT_FLAG_HAS_ID | TRACE_EVENT_FLAG_HAS_LOCAL_ID |
                       TRACE_EVENT_FLAG_HAS_GLOBAL_ID);
  if (id_flags) {
    if (event.has_scope()) {
      base::StringAppendF(out, ",\"scope\":\"%s\"", event.scope().c_str());
    }

    DCHECK(event.has_id());
    switch (id_flags) {
      case TRACE_EVENT_FLAG_HAS_ID:
        base::StringAppendF(out, ",\"id\":\"0x%" PRIx64 "\"",
                            static_cast<uint64_t>(event.id()));
        break;

      case TRACE_EVENT_FLAG_HAS_LOCAL_ID:
        base::StringAppendF(out, ",\"id2\":{\"local\":\"0x%" PRIx64 "\"}",
                            static_cast<uint64_t>(event.id()));
        break;

      case TRACE_EVENT_FLAG_HAS_GLOBAL_ID:
        base::StringAppendF(out, ",\"id2\":{\"global\":\"0x%" PRIx64 "\"}",
                            static_cast<uint64_t>(event.id()));
        break;

      default:
        NOTREACHED() << "More than one of the ID flags are set";
        break;
    }
  }

  if (event.flags() & TRACE_EVENT_FLAG_BIND_TO_ENCLOSING)
    base::StringAppendF(out, ",\"bp\":\"e\"");

  if (event.has_bind_id()) {
    base::StringAppendF(out, ",\"bind_id\":\"0x%" PRIx64 "\"",
                        static_cast<uint64_t>(event.bind_id()));
  }

  if (event.flags() & TRACE_EVENT_FLAG_FLOW_IN)
    base::StringAppendF(out, ",\"flow_in\":true");
  if (event.flags() & TRACE_EVENT_FLAG_FLOW_OUT)
    base::StringAppendF(out, ",\"flow_out\":true");

  // Instant events also output their scope.
  if (phase == TRACE_EVENT_PHASE_INSTANT) {
    char scope = '?';
    switch (event.flags() & TRACE_EVENT_FLAG_SCOPE_MASK) {
      case TRACE_EVENT_SCOPE_GLOBAL:
        scope = TRACE_EVENT_SCOPE_NAME_GLOBAL;
        break;

      case TRACE_EVENT_SCOPE_PROCESS:
        scope = TRACE_EVENT_SCOPE_NAME_PROCESS;
        break;

      case TRACE_EVENT_SCOPE_THREAD:
        scope = TRACE_EVENT_SCOPE_NAME_THREAD;
        break;
    }
    base::StringAppendF(out, ",\"s\":\"%c\"", scope);
  }

  *out += ",\"args\":{";
  for (int i = 0; i < event.args_size(); ++i) {
    auto& arg = event.args(i);

    if (i > 0) {
      *out += ",";
    }

    *out += "\"";
    *out += arg.has_name_index()
                ? GetStringFromStringTable(string_table, arg.name_index())
                : arg.name();
    *out += "\":";

    TraceEvent::TraceValue value;
    if (arg.has_bool_value()) {
      value.as_bool = arg.bool_value();
      TraceEvent::AppendValueAsJSON(TRACE_VALUE_TYPE_BOOL, value, out);
      continue;
    }

    if (arg.has_uint_value()) {
      value.as_uint = arg.uint_value();
      TraceEvent::AppendValueAsJSON(TRACE_VALUE_TYPE_UINT, value, out);
      continue;
    }

    if (arg.has_int_value()) {
      value.as_int = arg.int_value();
      TraceEvent::AppendValueAsJSON(TRACE_VALUE_TYPE_INT, value, out);
      continue;
    }

    if (arg.has_double_value()) {
      value.as_double = arg.double_value();
      TraceEvent::AppendValueAsJSON(TRACE_VALUE_TYPE_DOUBLE, value, out);
      continue;
    }

    if (arg.has_pointer_value()) {
      value.as_pointer = reinterpret_cast<void*>(arg.pointer_value());
      TraceEvent::AppendValueAsJSON(TRACE_VALUE_TYPE_POINTER, value, out);
      continue;
    }

    if (arg.has_string_value()) {
      std::string str = arg.string_value();
      value.as_string = &str[0];
      TraceEvent::AppendValueAsJSON(TRACE_VALUE_TYPE_STRING, value, out);
      continue;
    }

    if (arg.has_json_value()) {
      *out += arg.json_value();
      continue;
    }

    if (arg.has_traced_value()) {
      AppendProtoDictAsJSON(out, arg.traced_value());
      continue;
    }

    NOTREACHED();
  }

  *out += "}}";
}

}  // namespace

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

JSONTraceExporter::JSONTraceExporter(const std::string& config,
                                     perfetto::TracingService* service)
    : config_(config), metadata_(std::make_unique<base::DictionaryValue>()) {
  consumer_endpoint_ = service->ConnectConsumer(this, /*uid=*/0);

  // Start tracing.
  perfetto::TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(4096 * 100);

  auto* trace_event_config = trace_config.add_data_sources()->mutable_config();
  trace_event_config->set_name(mojom::kTraceEventDataSourceName);
  trace_event_config->set_target_buffer(0);
  auto* chrome_config = trace_event_config->mutable_chrome_config();
  chrome_config->set_trace_config(config_);

// Only CrOS and Cast support system tracing.
#if defined(OS_CHROMEOS) || (defined(IS_CHROMECAST) && defined(OS_LINUX))
  auto* system_trace_config = trace_config.add_data_sources()->mutable_config();
  system_trace_config->set_name(mojom::kSystemTraceDataSourceName);
  system_trace_config->set_target_buffer(0);
  auto* system_chrome_config = system_trace_config->mutable_chrome_config();
  system_chrome_config->set_trace_config(config_);
#endif

#if defined(OS_CHROMEOS)
  auto* arc_trace_config = trace_config.add_data_sources()->mutable_config();
  arc_trace_config->set_name(mojom::kArcTraceDataSourceName);
  arc_trace_config->set_target_buffer(0);
  auto* arc_chrome_config = arc_trace_config->mutable_chrome_config();
  arc_chrome_config->set_trace_config(config_);
#endif

  auto* trace_metadata_config =
      trace_config.add_data_sources()->mutable_config();
  trace_metadata_config->set_name(mojom::kMetaDataSourceName);
  trace_metadata_config->set_target_buffer(0);

  consumer_endpoint_->EnableTracing(trace_config);
}

JSONTraceExporter::~JSONTraceExporter() = default;

void JSONTraceExporter::OnConnect() {}

void JSONTraceExporter::OnDisconnect() {}

void JSONTraceExporter::OnTracingDisabled() {
  consumer_endpoint_->ReadBuffers();
}

// This is called by the Coordinator interface, mainly used by the
// TracingController which in turn is used by the tracing UI etc
// to start/stop tracing.
void JSONTraceExporter::StopAndFlush(OnTraceEventJSONCallback callback) {
  DCHECK(!json_callback_ && callback);
  json_callback_ = callback;

  consumer_endpoint_->DisableTracing();
}

void JSONTraceExporter::OnTraceData(std::vector<perfetto::TracePacket> packets,
                                    bool has_more) {
  DCHECK(json_callback_);
  DCHECK(!packets.empty() || !has_more);

  // Since we write each event before checking the limit, we'll
  // always go slightly over and hence we reserve some extra space
  // to avoid most reallocs.
  const size_t kReserveCapacity = kTraceEventBufferSizeInBytes * 5 / 4;
  std::string out;
  out.reserve(kReserveCapacity);

  if (!has_output_json_preamble_) {
    out = "{\"traceEvents\":[";
    has_output_json_preamble_ = true;
  }

  for (auto& encoded_packet : packets) {
    perfetto::protos::ChromeTracePacket packet;
    bool decoded = encoded_packet.Decode(&packet);
    DCHECK(decoded);

    if (!packet.has_chrome_events()) {
      continue;
    }

    auto& bundle = packet.chrome_events();

    std::unordered_map<int, std::string> string_table;
    for (auto& string_table_entry : bundle.string_table()) {
      string_table[string_table_entry.index()] = string_table_entry.value();
    }

    for (auto& event : bundle.trace_events()) {
      if (out.size() > kTraceEventBufferSizeInBytes) {
        json_callback_.Run(out, nullptr, true);
        out.clear();
      }

      if (has_output_first_event_) {
        out += ",\n";
      } else {
        has_output_first_event_ = true;
      }

      OutputJSONFromTraceEventProto(event, &out, string_table);
    }

    for (auto& metadata : bundle.metadata()) {
      if (metadata.has_string_value()) {
        metadata_->SetString(metadata.name(), metadata.string_value());
      } else if (metadata.has_int_value()) {
        metadata_->SetInteger(metadata.name(), metadata.int_value());
      } else if (metadata.has_bool_value()) {
        metadata_->SetBoolean(metadata.name(), metadata.bool_value());
      } else if (metadata.has_json_value()) {
        std::unique_ptr<base::Value> value(
            base::JSONReader::Read(metadata.json_value()));
        metadata_->Set(metadata.name(), std::move(value));
      } else {
        NOTREACHED();
      }
    }

    for (auto& legacy_ftrace_output : bundle.legacy_ftrace_output()) {
      legacy_system_ftrace_output_ += legacy_ftrace_output;
    }

    for (auto& legacy_json_trace : bundle.legacy_json_trace()) {
      // Tracing agents should only add this field when there is some data.
      DCHECK(!legacy_json_trace.data().empty());
      switch (legacy_json_trace.type()) {
        case perfetto::protos::ChromeLegacyJsonTrace::USER_TRACE:
          if (has_output_first_event_) {
            out += ",\n";
          } else {
            has_output_first_event_ = true;
          }
          out += legacy_json_trace.data();
          break;
        case perfetto::protos::ChromeLegacyJsonTrace::SYSTEM_TRACE:
          if (legacy_system_trace_events_.empty()) {
            legacy_system_trace_events_ = "{";
          } else {
            legacy_system_trace_events_ += ",";
          }
          legacy_system_trace_events_ += legacy_json_trace.data();
          break;
        default:
          NOTREACHED();
      }
    }
  }

  if (!has_more) {
    out += "]";

    if (!legacy_system_ftrace_output_.empty() ||
        !legacy_system_trace_events_.empty()) {
      // Should only have system events (e.g. ETW) or system ftrace output.
      DCHECK(legacy_system_ftrace_output_.empty() ||
             legacy_system_trace_events_.empty());
      out += ",\"systemTraceEvents\":";
      if (!legacy_system_ftrace_output_.empty()) {
        std::string escaped;
        base::EscapeJSONString(legacy_system_ftrace_output_,
                               true /* put_in_quotes */, &escaped);
        out += escaped;
      } else {
        out += legacy_system_trace_events_ + "}";
      }
    }

    if (!metadata_->empty()) {
      out += ",\"metadata\":";
      std::string json_value;
      base::JSONWriter::Write(*metadata_, &json_value);
      out += json_value;
    }

    out += "}";
  }

  json_callback_.Run(out, metadata_.get(), has_more);
}

// Consumer Detach / Attach is not used in Chrome.
void JSONTraceExporter::OnDetach(bool) {
  NOTREACHED();
}

void JSONTraceExporter::OnAttach(bool, const perfetto::TraceConfig&) {
  NOTREACHED();
}

void JSONTraceExporter::OnTraceStats(bool, const perfetto::TraceStats&) {
  // We don't currently use GetTraceStats().
  NOTREACHED();
}

}  // namespace tracing
