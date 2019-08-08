/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/trace_processor/export_json.h"
#include "src/trace_processor/metadata.h"

#include <json/value.h>
#include <json/writer.h>
#include <stdio.h>

#include "src/trace_processor/trace_storage.h"

namespace {

class TraceFormatWriter {
 public:
  TraceFormatWriter(FILE* output) : output_(output), first_event_(true) {
    WriteHeader();
  }

  ~TraceFormatWriter() { WriteFooter(); }

  void WriteSlice(int64_t begin_ts_us,
                  int64_t duration_us,
                  const char* cat,
                  const char* name,
                  uint32_t tid,
                  uint32_t pid) {
    if (!first_event_) {
      fputs(",", output_);
    }
    Json::FastWriter writer;
    Json::Value value;
    value["ph"] = "X";
    value["cat"] = cat;
    value["name"] = name;
    value["tid"] = Json::UInt(tid);
    value["pid"] = Json::UInt(pid);
    value["ts"] = Json::Int64(begin_ts_us);
    value["dur"] = Json::Int64(duration_us);
    fputs(writer.write(value).c_str(), output_);
    first_event_ = false;
  }

  void WriteMetadataEvent(const char* metadata_type,
                          const char* metadata_value,
                          uint32_t tid,
                          uint32_t pid) {
    if (!first_event_) {
      fputs(",", output_);
    }
    Json::FastWriter writer;
    Json::Value value;
    value["ph"] = "M";
    value["cat"] = "__metadata";
    value["ts"] = 0;
    value["name"] = metadata_type;
    value["tid"] = Json::UInt(tid);
    value["pid"] = Json::UInt(pid);

    Json::Value args;
    args["name"] = metadata_value;
    value["args"] = args;

    fputs(writer.write(value).c_str(), output_);
    first_event_ = false;
  }

  void AppendTelemetryMetadataString(const char* key, const char* value) {
    metadata_["telemetry"][key].append(value);
  }

  void AppendTelemetryMetadataInt(const char* key, int64_t value) {
    metadata_["telemetry"][key].append(Json::Int64(value));
  }

  void AppendTelemetryMetadataBool(const char* key, bool value) {
    metadata_["telemetry"][key].append(value);
  }

  void SetTelemetryMetadataTimestamp(const char* key, int64_t value) {
    metadata_["telemetry"][key] = value / 1000.0;
  }

 private:
  void WriteHeader() { fputs("{\"traceEvents\":[\n", output_); }

  void WriteFooter() {
    fputs("],\n\"metadata\":", output_);
    Json::FastWriter writer;
    fputs(writer.write(metadata_).c_str(), output_);
    fputs("\n}", output_);
    fflush(output_);
  }

  FILE* output_;
  bool first_event_;
  Json::Value metadata_;
};

}  // anonymous namespace

namespace perfetto {
namespace trace_processor {
namespace json {

ResultCode ExportJson(const TraceStorage* storage, FILE* output) {
  const StringPool& string_pool = storage->string_pool();

  TraceFormatWriter writer(output);

  // Write thread names.
  for (UniqueTid i = 1; i < storage->thread_count(); ++i) {
    auto thread = storage->GetThread(i);
    if (thread.name_id > 0) {
      const char* thread_name = string_pool.Get(thread.name_id).c_str();
      uint32_t pid = thread.upid ? storage->GetProcess(*thread.upid).pid : 0;
      writer.WriteMetadataEvent("thread_name", thread_name, thread.tid, pid);
    }
  }

  // Write process names.
  for (UniquePid i = 1; i < storage->process_count(); ++i) {
    auto process = storage->GetProcess(i);
    if (process.name_id > 0) {
      const char* process_name = string_pool.Get(process.name_id).c_str();
      writer.WriteMetadataEvent("process_name", process_name, 0, process.pid);
    }
  }

  // Write slices.
  const auto& slices = storage->nestable_slices();
  for (size_t i = 0; i < slices.slice_count(); ++i) {
    if (slices.types()[i] == RefType::kRefUtid) {
      int64_t begin_ts_us = slices.start_ns()[i] / 1000;
      int64_t duration_us = slices.durations()[i] / 1000;
      const char* cat = string_pool.Get(slices.cats()[i]).c_str();
      const char* name = string_pool.Get(slices.names()[i]).c_str();

      UniqueTid utid = static_cast<UniqueTid>(slices.refs()[i]);
      auto thread = storage->GetThread(utid);
      uint32_t pid = thread.upid ? storage->GetProcess(*thread.upid).pid : 0;

      writer.WriteSlice(begin_ts_us, duration_us, cat, name, thread.tid, pid);
    } else {
      return kResultWrongRefType;
    }
  }

  // Add metadata to be written in the footer
  const auto& trace_metadata = storage->metadata();
  if (!trace_metadata[metadata::benchmark_description].empty()) {
    auto desc_id =
        trace_metadata[metadata::benchmark_description][0].string_value;
    writer.AppendTelemetryMetadataString("benchmarkDescriptions",
                                         string_pool.Get(desc_id).c_str());
  }
  if (!trace_metadata[metadata::benchmark_name].empty()) {
    auto name_id = trace_metadata[metadata::benchmark_name][0].string_value;
    writer.AppendTelemetryMetadataString("benchmarks",
                                         string_pool.Get(name_id).c_str());
  }
  if (!trace_metadata[metadata::benchmark_start_time_us].empty()) {
    auto start_ts =
        trace_metadata[metadata::benchmark_start_time_us][0].int_value;
    writer.SetTelemetryMetadataTimestamp("benchmarkStart", start_ts);
  }
  if (!trace_metadata[metadata::benchmark_had_failures].empty()) {
    auto had_failures =
        trace_metadata[metadata::benchmark_had_failures][0].int_value;
    writer.AppendTelemetryMetadataBool("hadFailures", had_failures);
  }
  if (!trace_metadata[metadata::benchmark_label].empty()) {
    auto label_id = trace_metadata[metadata::benchmark_label][0].string_value;
    writer.AppendTelemetryMetadataString("labels",
                                         string_pool.Get(label_id).c_str());
  }
  if (!trace_metadata[metadata::benchmark_story_name].empty()) {
    auto name_id =
        trace_metadata[metadata::benchmark_story_name][0].string_value;
    writer.AppendTelemetryMetadataString("stories",
                                         string_pool.Get(name_id).c_str());
  }
  if (!trace_metadata[metadata::benchmark_story_run_index].empty()) {
    auto run_index =
        trace_metadata[metadata::benchmark_story_run_index][0].int_value;
    writer.AppendTelemetryMetadataInt("storysetRepeats", run_index);
  }
  if (!trace_metadata[metadata::benchmark_story_run_time_us].empty()) {
    auto story_ts =
        trace_metadata[metadata::benchmark_story_run_time_us][0].int_value;
    writer.SetTelemetryMetadataTimestamp("traceStart", story_ts);
  }
  for (auto tag : trace_metadata[metadata::benchmark_story_tags]) {
    auto tag_id = tag.string_value;
    writer.AppendTelemetryMetadataString("storyTags",
                                         string_pool.Get(tag_id).c_str());
  }

  return kResultOk;
}

}  // namespace json
}  // namespace trace_processor
}  // namespace perfetto
