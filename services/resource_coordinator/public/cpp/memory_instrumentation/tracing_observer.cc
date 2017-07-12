// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/memory_instrumentation/tracing_observer.h"

#include "base/memory/ptr_util.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event_argument.h"

namespace memory_instrumentation {

namespace {

const int kTraceEventNumArgs = 1;
const char* kTraceEventArgNames[] = {"dumps"};
const unsigned char kTraceEventArgTypes[] = {TRACE_VALUE_TYPE_CONVERTABLE};

bool IsMemoryInfraTracingEnabled() {
  bool enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      base::trace_event::MemoryDumpManager::kTraceCategory, &enabled);
  return enabled;
}

};  // namespace

TracingObserver::TracingObserver(
    base::trace_event::TraceLog* trace_log,
    base::trace_event::MemoryDumpManager* memory_dump_manager)
    : memory_dump_manager_(memory_dump_manager), trace_log_(trace_log) {
  // If tracing was enabled before initializing MemoryDumpManager, we missed the
  // OnTraceLogEnabled() event. Synthesize it so we can late-join the party.
  // IsEnabled is called before adding observer to avoid calling
  // OnTraceLogEnabled twice.
  bool is_tracing_already_enabled = trace_log_->IsEnabled();
  trace_log_->AddEnabledStateObserver(this);
  if (is_tracing_already_enabled)
    OnTraceLogEnabled();
}

TracingObserver::~TracingObserver() {
  trace_log_->RemoveEnabledStateObserver(this);
}

void TracingObserver::OnTraceLogEnabled() {
  if (!IsMemoryInfraTracingEnabled())
    return;

  // Initialize the TraceLog for the current thread. This is to avoids that the
  // TraceLog memory dump provider is registered lazily during the MDM
  // SetupForTracing().
  base::trace_event::TraceLog::GetInstance()
      ->InitializeThreadLocalEventBufferIfSupported();

  const base::trace_event::TraceConfig& trace_config =
      base::trace_event::TraceLog::GetInstance()->GetCurrentTraceConfig();
  const base::trace_event::TraceConfig::MemoryDumpConfig& memory_dump_config =
      trace_config.memory_dump_config();

  memory_dump_config_ =
      base::MakeUnique<base::trace_event::TraceConfig::MemoryDumpConfig>(
          memory_dump_config);

  memory_dump_manager_->SetupForTracing(memory_dump_config);
}

void TracingObserver::OnTraceLogDisabled() {
  memory_dump_manager_->TeardownForTracing();
  memory_dump_config_.reset();
}

bool TracingObserver::AddDumpToTraceIfEnabled(
    const base::trace_event::MemoryDumpRequestArgs* req_args,
    const base::ProcessId pid,
    const base::trace_event::ProcessMemoryDump* process_memory_dump) {
  // If tracing has been disabled early out to avoid the cost of serializing the
  // dump then ignoring the result.
  if (!IsMemoryInfraTracingEnabled())
    return false;
  // If the dump mode is too detailed don't add to trace to avoid accidentally
  // including PII.
  if (!IsDumpModeAllowed(req_args->level_of_detail))
    return false;

  CHECK_NE(base::trace_event::MemoryDumpType::SUMMARY_ONLY,
           req_args->dump_type);

  const uint64_t dump_guid = req_args->dump_guid;

  std::unique_ptr<base::trace_event::TracedValue> traced_value(
      new base::trace_event::TracedValue);
  process_memory_dump->AsValueInto(traced_value.get());
  traced_value->SetString("level_of_detail",
                          base::trace_event::MemoryDumpLevelOfDetailToString(
                              req_args->level_of_detail));
  const char* const event_name =
      base::trace_event::MemoryDumpTypeToString(req_args->dump_type);

  std::unique_ptr<base::trace_event::ConvertableToTraceFormat> event_value(
      std::move(traced_value));
  TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_PROCESS_ID(
      TRACE_EVENT_PHASE_MEMORY_DUMP,
      base::trace_event::TraceLog::GetCategoryGroupEnabled(
          base::trace_event::MemoryDumpManager::kTraceCategory),
      event_name, trace_event_internal::kGlobalScope, dump_guid, pid,
      kTraceEventNumArgs, kTraceEventArgNames, kTraceEventArgTypes,
      nullptr /* arg_values */, &event_value, TRACE_EVENT_FLAG_HAS_ID);

  return true;
}

bool TracingObserver::IsDumpModeAllowed(
    base::trace_event::MemoryDumpLevelOfDetail dump_mode) const {
  if (!memory_dump_config_)
    return false;
  return memory_dump_config_->allowed_dump_modes.count(dump_mode) != 0;
}

}  // namespace memory_instrumentation
