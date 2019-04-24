// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/perfetto/json_trace_exporter.h"

#include <utility>

#include "base/format_macros.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/trace_event/trace_event.h"
#include "services/tracing/public/cpp/trace_event_args_whitelist.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_packet.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_trace_event.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_trace_packet.pb.h"

namespace tracing {

namespace {

constexpr size_t kTraceEventBufferSizeInBytes = 100 * 1024;

}  // namespace

JSONTraceExporter::JSONTraceExporter(bool filter_args,
                                     OnTraceEventJSONCallback callback)
    : out_(callback),
      metadata_(std::make_unique<base::DictionaryValue>()),
      filter_args_(filter_args) {}

JSONTraceExporter::~JSONTraceExporter() = default;

void JSONTraceExporter::OnTraceData(std::vector<perfetto::TracePacket> packets,
                                    bool has_more) {
  DCHECK(!packets.empty() || !has_more);

  // TODO(eseckler): |label_filter_| seems broken for anything but
  // "traceEvents" (e.g. "systemTraceEvents" will output invalid JSON).
  if (label_filter_.empty() && !has_output_json_preamble_) {
    out_ += "{\"traceEvents\":[";
    has_output_json_preamble_ = true;
  }

  // Delegate to the subclasses to parse the packets. It will create
  // ScopedJSONTraceEventAppenders to write the contained events along with
  // other trace fields.
  ProcessPackets(packets);

  if (!has_more) {
    if (label_filter_.empty()) {
      // We are done adding events so now we close the traceEvents array and
      // append the rest of the fields. The rest of the fields aren't very large
      // so we don't need to check if we need to run the callback.
      out_ += "]";
    }

    if ((label_filter_.empty() || label_filter_ == "systemTraceEvents") &&
        (!legacy_system_ftrace_output_.empty() ||
         !legacy_system_trace_events_.empty())) {
      DCHECK(legacy_system_ftrace_output_.empty() ||
             legacy_system_trace_events_.empty());
      out_ += ",\"systemTraceEvents\":";
      if (!legacy_system_ftrace_output_.empty()) {
        std::string escaped;
        base::EscapeJSONString(legacy_system_ftrace_output_,
                               true /* put_in_quotes */, &escaped);
        out_ += escaped;
      } else {
        out_ += legacy_system_trace_events_ + "}";
      }
    }

    if (label_filter_.empty()) {
      if (!metadata_->empty()) {
        out_ += ",\"metadata\":";
        std::string json_value;
        base::JSONWriter::Write(*metadata_, &json_value);
        out_ += json_value;
      }

      // Finish the json object we started in the preamble.
      out_ += "}";
    }
  }

  // Send any remaining data. There is no harm issuing the callback with an
  // empty string, so we do it unconditionally.
  out_.Flush(metadata_.get(), has_more);
}

bool JSONTraceExporter::ShouldOutputTraceEvents() const {
  return label_filter_.empty() || label_filter_ == "traceEvents";
}

void JSONTraceExporter::AddChromeLegacyJSONTrace(
    const perfetto::protos::ChromeLegacyJsonTrace& json_trace) {
  // Tracing agents should only add this field when there is some data.
  DCHECK(!json_trace.data().empty());
  switch (json_trace.type()) {
    case perfetto::protos::ChromeLegacyJsonTrace::USER_TRACE:
      *AddJSONTraceEvent() += json_trace.data();
      return;
    case perfetto::protos::ChromeLegacyJsonTrace::SYSTEM_TRACE:
      if (!ShouldOutputTraceEvents()) {
        return;
      }
      if (legacy_system_trace_events_.empty()) {
        legacy_system_trace_events_ = "{";
      } else {
        legacy_system_trace_events_ += ",";
      }
      legacy_system_trace_events_ += json_trace.data();
      return;
    default:
      NOTREACHED();
  }
}

void JSONTraceExporter::AddLegacyFtrace(
    const std::string& legacy_ftrace_output) {
  legacy_system_ftrace_output_ += legacy_ftrace_output;
}

void JSONTraceExporter::AddChromeMetadata(
    const perfetto::protos::ChromeMetadata& metadata) {
  if (metadata.has_string_value()) {
    metadata_->SetString(metadata.name(), metadata.string_value());
  } else if (metadata.has_int_value()) {
    metadata_->SetInteger(metadata.name(), metadata.int_value());
  } else if (metadata.has_bool_value()) {
    metadata_->SetBoolean(metadata.name(), metadata.bool_value());
  } else if (metadata.has_json_value()) {
    std::unique_ptr<base::Value> value(
        base::JSONReader::ReadDeprecated(metadata.json_value()));
    metadata_->Set(metadata.name(), std::move(value));
  } else {
    NOTREACHED();
  }
}

void JSONTraceExporter::SetTraceStatsMetadata(
    const perfetto::protos::TraceStats& trace_stats) {
  auto dict = std::make_unique<base::DictionaryValue>();
  dict->SetInteger("producers_connected", trace_stats.producers_connected());
  dict->SetInteger("producers_seen", trace_stats.producers_seen());
  dict->SetInteger("data_sources_registered",
                   trace_stats.data_sources_registered());
  dict->SetInteger("data_sources_seen", trace_stats.data_sources_seen());
  dict->SetInteger("tracing_sessions", trace_stats.tracing_sessions());
  dict->SetInteger("total_buffers", trace_stats.total_buffers());
  dict->SetInteger("chunks_discarded", trace_stats.chunks_discarded());
  dict->SetInteger("patches_discarded", trace_stats.patches_discarded());
  auto buf_list = std::make_unique<base::ListValue>();
  for (const auto& buf_stats : trace_stats.buffer_stats()) {
    base::Value buf_value(base::Value::Type::DICTIONARY);
    base::DictionaryValue* buf_dict;
    buf_value.GetAsDictionary(&buf_dict);
    buf_dict->SetInteger("buffer_size", buf_stats.buffer_size());
    buf_dict->SetInteger("bytes_written", buf_stats.bytes_written());
    buf_dict->SetInteger("bytes_overwritten", buf_stats.bytes_overwritten());
    buf_dict->SetInteger("bytes_read", buf_stats.bytes_read());
    buf_dict->SetInteger("padding_bytes_written",
                         buf_stats.padding_bytes_written());
    buf_dict->SetInteger("padding_bytes_cleared",
                         buf_stats.padding_bytes_cleared());
    buf_dict->SetInteger("chunks_written", buf_stats.chunks_written());
    buf_dict->SetInteger("chunks_rewritten", buf_stats.chunks_rewritten());
    buf_dict->SetInteger("chunks_overwritten", buf_stats.chunks_overwritten());
    buf_dict->SetInteger("chunks_discarded", buf_stats.chunks_discarded());
    buf_dict->SetInteger("chunks_read", buf_stats.chunks_read());
    buf_dict->SetInteger("chunks_committed_out_of_order",
                         buf_stats.chunks_committed_out_of_order());
    buf_dict->SetInteger("write_wrap_count", buf_stats.write_wrap_count());
    buf_dict->SetInteger("patches_succeeded", buf_stats.patches_succeeded());
    buf_dict->SetInteger("patches_failed", buf_stats.patches_failed());
    buf_dict->SetInteger("readaheads_succeeded",
                         buf_stats.readaheads_succeeded());
    buf_dict->SetInteger("readaheads_failed", buf_stats.readaheads_failed());
    buf_dict->SetInteger("abi_violations", buf_stats.abi_violations());
    buf_list->GetList().push_back(std::move(buf_value));
  }
  dict->SetList("buffer_stats", std::move(buf_list));
  metadata_->SetDictionary("perfetto_trace_stats", std::move(dict));
}

JSONTraceExporter::ScopedJSONTraceEventAppender
JSONTraceExporter::AddTraceEvent(const char* name,
                                 const char* categories,
                                 int32_t phase,
                                 int64_t timestamp,
                                 int32_t pid,
                                 int32_t tid) {
  DCHECK(ShouldOutputTraceEvents());
  return JSONTraceExporter::ScopedJSONTraceEventAppender(
      AddJSONTraceEvent(), filter_args_, name, categories, phase, timestamp,
      pid, tid);
}

JSONTraceExporter::StringBuffer* JSONTraceExporter::AddJSONTraceEvent() {
  // If we've already added the first value in the TraceEvent array then we need
  // a comma and add a newline for readability. Otherwise we're fine but update
  // so in future we know we've finished the first event.
  if (has_output_first_event_) {
    out_ += ",\n";
  } else {
    has_output_first_event_ = true;
  }
  return &out_;
}

JSONTraceExporter::StringBuffer::StringBuffer(
    JSONTraceExporter::OnTraceEventJSONCallback callback)
    : callback_(std::move(callback)) {
  // Since we write each string before checking the limit, we'll
  // always go slightly over and hence we reserve some extra space
  // to avoid most reallocs.
  const size_t kReserveCapacity = kTraceEventBufferSizeInBytes * 5 / 4;
  out_.reserve(kReserveCapacity);
}

JSONTraceExporter::StringBuffer::~StringBuffer() {}

JSONTraceExporter::StringBuffer& JSONTraceExporter::StringBuffer::operator+=(
    const std::string& input) {
  MaybeRunCallback();
  out_ += input;
  return *this;
}

JSONTraceExporter::StringBuffer& JSONTraceExporter::StringBuffer::operator+=(
    std::string&& input) {
  MaybeRunCallback();
  out_ += std::move(input);
  return *this;
}

JSONTraceExporter::StringBuffer& JSONTraceExporter::StringBuffer::operator+=(
    const char* input) {
  MaybeRunCallback();
  out_ += input;
  return *this;
}

std::string* JSONTraceExporter::StringBuffer::mutable_out() {
  return &out_;
}

const std::string& JSONTraceExporter::StringBuffer::out() {
  return out_;
}

void JSONTraceExporter::StringBuffer::EscapeJSONAndAppend(
    const std::string& unescaped) {
  MaybeRunCallback();
  base::EscapeJSONString(unescaped, true, &out_);
}

void JSONTraceExporter::StringBuffer::Flush(base::DictionaryValue* metadata,
                                            bool has_more) {
  callback_.Run(out_, metadata, has_more);
  if (has_more) {
    // We clear |out_| because we've processed all the current data in |out_|
    // and we don't want any data to be repeated. We have to protect this by
    // checking |has_more| because the callback could have deleted |this| in
    // which cause |out_| is a destroyed as well.
    out_.clear();
  }
}

void JSONTraceExporter::StringBuffer::MaybeRunCallback() {
  // If the string has exceeded the threshold we send the current part back
  // through the callback and clear the string to prevent it from getting to
  // large.
  if (out_.size() > kTraceEventBufferSizeInBytes) {
    Flush(nullptr, true);
  }
}

JSONTraceExporter::ArgumentBuilder::ArgumentBuilder(
    bool filter_args,
    const char* name,
    const char* category_group_name,
    StringBuffer* out)
    : out_(out) {
  strip_args_ = filter_args &&
                !IsTraceEventArgsWhitelisted(category_group_name, name,
                                             &argument_name_filter_predicate_);
  *out_ += ",\"args\":";
}

JSONTraceExporter::ArgumentBuilder::~ArgumentBuilder() {
  if (strip_args_ && has_args_) {
    *out_ += "\"__stripped__\"";
  } else if (!has_args_) {
    *out_ += "{}";
  } else {
    *out_ += "}";
  }
}

JSONTraceExporter::StringBuffer*
JSONTraceExporter::ArgumentBuilder::MaybeAddArg(const std::string& name) {
  if (SkipBecauseStripped(name)) {
    return nullptr;
  }
  auto* buffer = AddArg();
  buffer->AppendF("\"%s\":", name.c_str());
  return buffer;
}

JSONTraceExporter::StringBuffer* JSONTraceExporter::ArgumentBuilder::AddArg() {
  if (has_args_) {
    *out_ += ",";
  } else {
    *out_ += "{";
    has_args_ = true;
  }
  return out_;
}

bool JSONTraceExporter::ArgumentBuilder::ArgumentNameIsStripped(
    const std::string& name) {
  return !argument_name_filter_predicate_.is_null() &&
         !argument_name_filter_predicate_.Run(name.c_str());
}

bool JSONTraceExporter::ArgumentBuilder::SkipBecauseStripped(
    const std::string& name) {
  if (strip_args_) {
    has_args_ = true;
    return true;
  }
  if (ArgumentNameIsStripped(name)) {
    AddArg()->AppendF("\"%s\":\"__stripped__\"", name.c_str());
    return true;
  }
  return false;
}

JSONTraceExporter::ScopedJSONTraceEventAppender::ScopedJSONTraceEventAppender(
    JSONTraceExporter::StringBuffer* out,
    bool filter_args,
    const char* name,
    const char* categories,
    int32_t phase,
    int64_t timestamp,
    int32_t pid,
    int32_t tid)
    : phase_(static_cast<char>(phase)),
      added_args_(false),
      out_(out),
      event_name_(name),
      category_group_name_(categories),
      filter_args_(filter_args) {
  out_->AppendF("{\"pid\":%i,\"tid\":%i,\"ts\":%" PRId64
                ",\"ph\":\"%c\",\"cat\":\"%s\",\"name\":",
                pid, tid, timestamp, phase_, categories);
  out_->EscapeJSONAndAppend(name);
}

JSONTraceExporter::ScopedJSONTraceEventAppender::ScopedJSONTraceEventAppender(
    JSONTraceExporter::ScopedJSONTraceEventAppender&& move) {
  out_ = move.out_;
  phase_ = move.phase_;
  filter_args_ = move.filter_args_;
  // We null out the string so that the destructor knows not to append the
  // closing brace for the json.
  move.out_ = nullptr;
}

JSONTraceExporter::ScopedJSONTraceEventAppender::
    ~ScopedJSONTraceEventAppender() {
  if (out_) {
    // TraceEventAnalyzer expects |args| to exist even if it's empty.
    if (!added_args_) {
      *out_ += ",\"args\":{}";
    }
    // Close out the starting bracket.
    *out_ += "}";
  }
}

void JSONTraceExporter::ScopedJSONTraceEventAppender::AddDuration(
    int64_t duration) {
  out_->AppendF(",\"dur\":%" PRId64, duration);
}

void JSONTraceExporter::ScopedJSONTraceEventAppender::AddThreadDuration(
    int64_t thread_duration) {
  out_->AppendF(",\"tdur\":%" PRId64, thread_duration);
}

void JSONTraceExporter::ScopedJSONTraceEventAppender::AddThreadTimestamp(
    int64_t thread_timestamp) {
  out_->AppendF(",\"tts\":%" PRId64, thread_timestamp);
}

void JSONTraceExporter::ScopedJSONTraceEventAppender::AddBindId(
    uint64_t bind_id) {
  out_->AppendF(",\"bind_id\":\"0x%" PRIx64 "\"",
                static_cast<uint64_t>(bind_id));
}

void JSONTraceExporter::ScopedJSONTraceEventAppender::AddFlags(
    uint32_t flags,
    base::Optional<uint64_t> id,
    const std::string& scope) {
  if (flags & TRACE_EVENT_FLAG_ASYNC_TTS) {
    *out_ += ",\"use_async_tts\":1";
  }

  // If |id| is set, print it out as a hex string so we don't loose any
  // bits (it might be a 64-bit pointer).
  unsigned int id_flags =
      flags & (TRACE_EVENT_FLAG_HAS_ID | TRACE_EVENT_FLAG_HAS_LOCAL_ID |
               TRACE_EVENT_FLAG_HAS_GLOBAL_ID);
  if (id_flags) {
    if (!scope.empty()) {
      out_->AppendF(",\"scope\":\"%s\"", scope.c_str());
    }

    DCHECK(id);
    switch (id_flags) {
      case TRACE_EVENT_FLAG_HAS_ID:
        out_->AppendF(",\"id\":\"0x%" PRIx64 "\"", static_cast<uint64_t>(*id));
        break;

      case TRACE_EVENT_FLAG_HAS_LOCAL_ID:
        out_->AppendF(",\"id2\":{\"local\":\"0x%" PRIx64 "\"}",
                      static_cast<uint64_t>(*id));
        break;

      case TRACE_EVENT_FLAG_HAS_GLOBAL_ID:
        out_->AppendF(",\"id2\":{\"global\":\"0x%" PRIx64 "\"}",
                      static_cast<uint64_t>(*id));
        break;

      default:
        NOTREACHED() << "More than one of the ID flags are set";
        break;
    }
  }

  if (flags & TRACE_EVENT_FLAG_BIND_TO_ENCLOSING)
    *out_ += ",\"bp\":\"e\"";

  if (flags & TRACE_EVENT_FLAG_FLOW_IN)
    *out_ += ",\"flow_in\":true";
  if (flags & TRACE_EVENT_FLAG_FLOW_OUT)
    *out_ += ",\"flow_out\":true";

  // Instant events also output their scope.
  if (phase_ == TRACE_EVENT_PHASE_INSTANT) {
    char scope = '?';
    switch (flags & TRACE_EVENT_FLAG_SCOPE_MASK) {
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
    out_->AppendF(",\"s\":\"%c\"", scope);
  }
}

std::unique_ptr<JSONTraceExporter::ArgumentBuilder>
JSONTraceExporter::ScopedJSONTraceEventAppender::BuildArgs() {
  DCHECK(!added_args_);
  added_args_ = true;
  return std::make_unique<ArgumentBuilder>(filter_args_, event_name_,
                                           category_group_name_, out_);
}
}  // namespace tracing
