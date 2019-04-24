// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PERFETTO_JSON_TRACE_EXPORTER_H_
#define SERVICES_TRACING_PERFETTO_JSON_TRACE_EXPORTER_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_packet.h"

namespace perfetto {
namespace protos {
class ChromeLegacyJsonTrace;
class ChromeMetadata;
class TraceStats;
}  // namespace protos
}  // namespace perfetto

namespace tracing {

// Converts proto-encoded trace data into the legacy JSON trace format.
// Conversion happens on-the-fly as new trace packets are received.
class JSONTraceExporter {
 public:
  // Given argument name for the trace event, returns if the argument should be
  // filtered or not.
  using ArgumentNameFilterPredicate =
      base::RepeatingCallback<bool(const char* arg_name)>;

  using OnTraceEventJSONCallback =
      base::RepeatingCallback<void(const std::string& json,
                                   base::DictionaryValue* metadata,
                                   bool has_more)>;

  JSONTraceExporter(bool filter_args, OnTraceEventJSONCallback callback);
  virtual ~JSONTraceExporter();

  // Called to notify the exporter of new trace packets. Will call the
  // |json_callback| passed in the constructor with the converted trace data.
  void OnTraceData(std::vector<perfetto::TracePacket> packets, bool has_more);

  void set_filter_args_for_testing(bool value) { filter_args_ = value; }

  void set_label_filter(const std::string& label_filter) {
    label_filter_ = label_filter;
  }

 protected:
  class StringBuffer {
   public:
    StringBuffer(OnTraceEventJSONCallback callback);
    StringBuffer(const StringBuffer& copy) = delete;
    StringBuffer(StringBuffer&& move);
    ~StringBuffer();

    StringBuffer& operator+=(const std::string& input);

    StringBuffer& operator+=(std::string&& input);

    StringBuffer& operator+=(const char* input);

    std::string* mutable_out();
    const std::string& out();

    template <typename... Args>
    void AppendF(const char* format, Args&&... args) {
      MaybeRunCallback();
      base::StringAppendF(&out_, format, std::forward<Args>(args)...);
    }

    void EscapeJSONAndAppend(const std::string& unescaped);

    void Flush(base::DictionaryValue* metadata, bool has_more);

   private:
    // Depending on the size of the current output we might need to send a part
    // of it back.
    void MaybeRunCallback();

    std::string out_;
    OnTraceEventJSONCallback callback_;
  };

  class ArgumentBuilder {
   public:
    ArgumentBuilder(bool filter_args,
                    const char* name,
                    const char* category_group_name,
                    StringBuffer* out);
    ~ArgumentBuilder();

    // Takes an arg name, and returns nullptr if
    //
    // a) all args are being stripped
    // b) if this arg name was stripped.
    //
    // If the StringBuffer pointer is valid then you should append a string that
    // is properly formatted json for this arg value.
    StringBuffer* MaybeAddArg(const std::string& name);

   private:
    StringBuffer* AddArg();
    bool ArgumentNameIsStripped(const std::string& name);
    bool SkipBecauseStripped(const std::string& name);

    StringBuffer* out_;
    bool strip_args_ = false;
    bool has_args_ = false;
    ArgumentNameFilterPredicate argument_name_filter_predicate_;
  };

  // Adds all required fields to |out| in proper JSON format. Only one
  // ScopedJSONTraceEventAppender should exist per |out| string at a time,
  // since the TraceEvent will not be finished until the
  // ScopedJSONTraceEventAppender goes out of scope.
  class ScopedJSONTraceEventAppender {
   public:
    // Only one reference should exist at a time. So moving is okay but copying
    // is disallowed.
    ScopedJSONTraceEventAppender(ScopedJSONTraceEventAppender&& move);
    ScopedJSONTraceEventAppender(const ScopedJSONTraceEventAppender& copy) =
        delete;

    // Ensures that the JSON object is properly closed.
    ~ScopedJSONTraceEventAppender();

    // Optional traceEvent fields can also be set with the methods below. All
    // methods should only be called once.

    void AddDuration(int64_t duration);
    void AddThreadDuration(int64_t thread_duration);
    void AddThreadTimestamp(int64_t thread_timestamp);
    void AddBindId(uint64_t bind_id);
    // A set of bit flags for this trace event, along with a |scope|. |scope| is
    // ignored if empty.
    void AddFlags(uint32_t flags,
                  base::Optional<uint64_t> id,
                  const std::string& scope);

    // Begins constructing the args sections, and finishes when ArgumentBuilder
    // is destroyed. No other Add* function should be called until
    // ArgumentBuilder goes out of scope.
    //
    // This should be used as follows.
    // {
    //   auto arg_builder = scoped_appender.BuildArgs();
    //   for (const auto& arg : args) {
    //     JSONTraceExporter::StringBuffer* maybe_arg =
    //         arg_builder->MaybeAddArg(arg.name);
    //     if (maybe_arg) {
    //       // Then one of the following to add the value in |arg|.
    //       *maybe_arg += "\"json_formatted_raw_value\"";
    //       maybe_arg->AppendF("\"%d\"", arg.integer);
    //       maybe_arg->EscapeJSONAndAppend("json_that will be : escaped");
    //     }
    //   }
    // }
    //
    // IMPORTANT: ArgumentBuilder must be deconstructed before the
    // ScopedJSONTraceEventAppender that created it is.
    std::unique_ptr<ArgumentBuilder> BuildArgs();

   private:
    // Subclasses of JSONTraceExporter can create a new instance by calling
    // AddTraceEvent().
    ScopedJSONTraceEventAppender(StringBuffer* out,
                                 bool filter_args,
                                 const char* name,
                                 const char* categories,
                                 int32_t phase,
                                 int64_t timestamp,
                                 int32_t pid,
                                 int32_t tid);
    friend class JSONTraceExporter;

    char phase_;
    bool added_args_;
    StringBuffer* out_;
    const char* event_name_;
    const char* category_group_name_;
    bool filter_args_;
  };

  // Subclasses implement this to add data from |packets| to the JSON output.
  // For example they can add traceEvents through AddTraceEvent(), or add
  // metadata through AddChromeMetadata().
  virtual void ProcessPackets(
      const std::vector<perfetto::TracePacket>& packets) = 0;

  // If true then all trace events should be skipped. AddTraceEvent should not
  // be called.
  bool ShouldOutputTraceEvents() const;

  // Used for passing legacy JSON traces. This will update either the
  // traceEvents directly if needed by calling AddJSONTraceEvent or will store
  // the system trace information to be appended after the packets have been
  // processed.
  void AddChromeLegacyJSONTrace(
      const perfetto::protos::ChromeLegacyJsonTrace& json_trace);
  // Adds system Ftrace data to be appended to the trace JSON after all the
  // traceEvents have been processed.
  void AddLegacyFtrace(const std::string& legacy_ftrace_output);
  // Used to append ChromeMetadata to the trace. Can be called at any point.
  // Metadata is always appended after all packets have been processed.
  void AddChromeMetadata(const perfetto::protos::ChromeMetadata& metadata);
  // Writes (overwriting if already set) the perfetto trace stats to the
  // metadata that will be appended after all packets have been processed.
  void SetTraceStatsMetadata(const perfetto::protos::TraceStats& stats);

  // Used when sub-classes are adding a new trace event to the traceEvents
  // array. This will ensure that only proper json is appended.
  ScopedJSONTraceEventAppender AddTraceEvent(const char* name,
                                             const char* categories,
                                             int32_t phase,
                                             int64_t timestamp,
                                             int32_t pid,
                                             int32_t tid);

 private:
  // Used by the implementation to ensure the proper separators exist between
  // trace events in the array.
  StringBuffer* AddJSONTraceEvent();

  StringBuffer out_;
  bool has_output_first_event_ = false;
  bool has_output_json_preamble_ = false;
  std::string legacy_system_trace_events_;
  std::string label_filter_;
  std::string legacy_system_ftrace_output_;
  std::unique_ptr<base::DictionaryValue> metadata_;
  bool filter_args_;

  DISALLOW_COPY_AND_ASSIGN(JSONTraceExporter);
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PERFETTO_JSON_TRACE_EXPORTER_H_
