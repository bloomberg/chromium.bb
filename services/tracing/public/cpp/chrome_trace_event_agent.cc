// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/chrome_trace_event_agent.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/trace_event/trace_log.h"
#include "base/values.h"
#include "build/build_config.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/tracing/public/mojom/constants.mojom.h"

namespace {

const char kChromeTraceEventLabel[] = "traceEvents";

tracing::ChromeTraceEventAgent* g_chrome_trace_event_agent;

}  // namespace

namespace tracing {

// static
ChromeTraceEventAgent* ChromeTraceEventAgent::GetInstance() {
  return g_chrome_trace_event_agent;
}

ChromeTraceEventAgent::ChromeTraceEventAgent(
    service_manager::Connector* connector,
    bool request_clock_sync_marker_on_android)
    : BaseAgent(connector,
                kChromeTraceEventLabel,
                mojom::TraceDataType::ARRAY,
#if defined(OS_ANDROID)
                request_clock_sync_marker_on_android),
#else
                false),
#endif
      enabled_tracing_modes_(0) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!g_chrome_trace_event_agent);
  g_chrome_trace_event_agent = this;
}

ChromeTraceEventAgent::~ChromeTraceEventAgent() {
  DCHECK(!trace_log_needs_me_);
  g_chrome_trace_event_agent = nullptr;
}

void ChromeTraceEventAgent::AddMetadataGeneratorFunction(
    MetadataGeneratorFunction generator) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  metadata_generator_functions_.push_back(generator);
}

void ChromeTraceEventAgent::StartTracing(const std::string& config,
                                         base::TimeTicks coordinator_time,
                                         const StartTracingCallback& callback) {
  DCHECK(!recorder_);
#if defined(__native_client__)
  // NaCl and system times are offset by a bit, so subtract some time from
  // the captured timestamps. The value might be off by a bit due to messaging
  // latency.
  base::TimeDelta time_offset = TRACE_TIME_TICKS_NOW() - coordinator_time;
  TraceLog::GetInstance()->SetTimeOffset(time_offset);
#endif
  enabled_tracing_modes_ = base::trace_event::TraceLog::RECORDING_MODE;
  const base::trace_event::TraceConfig trace_config(config);
  if (!trace_config.event_filters().empty())
    enabled_tracing_modes_ |= base::trace_event::TraceLog::FILTERING_MODE;
  base::trace_event::TraceLog::GetInstance()->SetEnabled(
      trace_config, enabled_tracing_modes_);
  callback.Run(true);
}

void ChromeTraceEventAgent::StopAndFlush(mojom::RecorderPtr recorder) {
  DCHECK(!recorder_);
  recorder_ = std::move(recorder);
  base::trace_event::TraceLog::GetInstance()->SetDisabled(
      enabled_tracing_modes_);
  enabled_tracing_modes_ = 0;
  for (const auto& generator : metadata_generator_functions_) {
    auto metadata = generator.Run();
    if (metadata)
      recorder_->AddMetadata(std::move(metadata));
  }
  trace_log_needs_me_ = true;
  base::trace_event::TraceLog::GetInstance()->Flush(base::Bind(
      &ChromeTraceEventAgent::OnTraceLogFlush, base::Unretained(this)));
}

void ChromeTraceEventAgent::RequestClockSyncMarker(
    const std::string& sync_id,
    const Agent::RequestClockSyncMarkerCallback& callback) {
#if defined(OS_ANDROID)
  base::trace_event::TraceLog::GetInstance()->AddClockSyncMetadataEvent();
  callback.Run(base::TimeTicks(), base::TimeTicks());
#else
  NOTREACHED();
#endif
}

void ChromeTraceEventAgent::RequestBufferStatus(
    const RequestBufferStatusCallback& callback) {
  base::trace_event::TraceLogStatus status =
      base::trace_event::TraceLog::GetInstance()->GetStatus();
  callback.Run(status.event_capacity, status.event_count);
}

void ChromeTraceEventAgent::GetCategories(
    const GetCategoriesCallback& callback) {
  std::vector<std::string> category_vector;
  base::trace_event::TraceLog::GetInstance()->GetKnownCategoryGroups(
      &category_vector);
  callback.Run(base::JoinString(category_vector, ","));
}

void ChromeTraceEventAgent::OnTraceLogFlush(
    const scoped_refptr<base::RefCountedString>& events_str,
    bool has_more_events) {
  if (!events_str->data().empty())
    recorder_->AddChunk(events_str->data());
  if (!has_more_events) {
    trace_log_needs_me_ = false;
    recorder_.reset();
  }
}

}  // namespace tracing
