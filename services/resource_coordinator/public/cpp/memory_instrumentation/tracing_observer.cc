// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/memory_instrumentation/tracing_observer.h"

#include "base/files/file_path.h"
#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event_argument.h"

namespace memory_instrumentation {

using base::trace_event::TracedValue;
using base::trace_event::ProcessMemoryDump;

namespace {

const int kTraceEventNumArgs = 1;
const char* const kTraceEventArgNames[] = {"dumps"};
const unsigned char kTraceEventArgTypes[] = {TRACE_VALUE_TYPE_CONVERTABLE};

bool IsMemoryInfraTracingEnabled() {
  bool enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      base::trace_event::MemoryDumpManager::kTraceCategory, &enabled);
  return enabled;
}

void OsDumpAsValueInto(TracedValue* value, const mojom::OSMemDump& os_dump) {
  value->SetString(
      "resident_set_bytes",
      base::StringPrintf("%" PRIx32, os_dump.resident_set_kb * 1024));
  value->SetString(
      "private_footprint_bytes",
      base::StringPrintf("%" PRIx32, os_dump.private_footprint_kb * 1024));
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
      std::make_unique<base::trace_event::TraceConfig::MemoryDumpConfig>(
          memory_dump_config);

  if (memory_dump_manager_)
    memory_dump_manager_->SetupForTracing(memory_dump_config);
}

void TracingObserver::OnTraceLogDisabled() {
  if (memory_dump_manager_)
    memory_dump_manager_->TeardownForTracing();
  memory_dump_config_.reset();
}

bool TracingObserver::ShouldAddToTrace(
    const base::trace_event::MemoryDumpRequestArgs& args) {
  // If tracing has been disabled early out to avoid the cost of serializing the
  // dump then ignoring the result.
  if (!IsMemoryInfraTracingEnabled())
    return false;
  // If the dump mode is too detailed don't add to trace to avoid accidentally
  // including PII.
  if (!IsDumpModeAllowed(args.level_of_detail))
    return false;
  return true;
}

void TracingObserver::AddToTrace(
    const base::trace_event::MemoryDumpRequestArgs& args,
    const base::ProcessId pid,
    std::unique_ptr<TracedValue> traced_value) {
  CHECK_NE(base::trace_event::MemoryDumpType::SUMMARY_ONLY, args.dump_type);

  traced_value->SetString(
      "level_of_detail",
      base::trace_event::MemoryDumpLevelOfDetailToString(args.level_of_detail));
  const uint64_t dump_guid = args.dump_guid;
  const char* const event_name =
      base::trace_event::MemoryDumpTypeToString(args.dump_type);
  std::unique_ptr<base::trace_event::ConvertableToTraceFormat> event_value(
      std::move(traced_value));
  TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_PROCESS_ID(
      TRACE_EVENT_PHASE_MEMORY_DUMP,
      base::trace_event::TraceLog::GetCategoryGroupEnabled(
          base::trace_event::MemoryDumpManager::kTraceCategory),
      event_name, trace_event_internal::kGlobalScope, dump_guid, pid,
      kTraceEventNumArgs, kTraceEventArgNames, kTraceEventArgTypes,
      nullptr /* arg_values */, &event_value, TRACE_EVENT_FLAG_HAS_ID);
}

bool TracingObserver::AddChromeDumpToTraceIfEnabled(
    const base::trace_event::MemoryDumpRequestArgs& args,
    const base::ProcessId pid,
    const ProcessMemoryDump* process_memory_dump) {
  if (!ShouldAddToTrace(args))
    return false;

  std::unique_ptr<TracedValue> traced_value = std::make_unique<TracedValue>();
  process_memory_dump->SerializeAllocatorDumpsInto(traced_value.get());

  AddToTrace(args, pid, std::move(traced_value));

  return true;
}

bool TracingObserver::AddOsDumpToTraceIfEnabled(
    const base::trace_event::MemoryDumpRequestArgs& args,
    const base::ProcessId pid,
    const mojom::OSMemDump* os_dump,
    const std::vector<mojom::VmRegionPtr>* memory_maps) {
  if (!ShouldAddToTrace(args))
    return false;

  std::unique_ptr<TracedValue> traced_value = std::make_unique<TracedValue>();

  traced_value->BeginDictionary("process_totals");
  OsDumpAsValueInto(traced_value.get(), *os_dump);
  traced_value->EndDictionary();

  if (memory_maps->size()) {
    traced_value->BeginDictionary("process_mmaps");
    MemoryMapsAsValueInto(*memory_maps, traced_value.get(), false);
    traced_value->EndDictionary();
  }

  AddToTrace(args, pid, std::move(traced_value));
  return true;
}

// static
void TracingObserver::MemoryMapsAsValueInto(
    const std::vector<mojom::VmRegionPtr>& memory_maps,
    TracedValue* value,
    bool is_argument_filtering_enabled) {
  static const char kHexFmt[] = "%" PRIx64;

  // Refer to the design doc goo.gl/sxfFY8 for the semantics of these fields.
  value->BeginArray("vm_regions");
  for (const auto& region : memory_maps) {
    value->BeginDictionary();

    value->SetString("sa", base::StringPrintf(kHexFmt, region->start_address));
    value->SetString("sz", base::StringPrintf(kHexFmt, region->size_in_bytes));
    if (region->module_timestamp)
      value->SetString("ts",
                       base::StringPrintf(kHexFmt, region->module_timestamp));
    value->SetInteger("pf", region->protection_flags);

    // The module path will be the basename when argument filtering is
    // activated. The whitelisting implemented for filtering string values
    // doesn't allow rewriting. Therefore, a different path is produced here
    // when argument filtering is activated.
    if (is_argument_filtering_enabled) {
      base::FilePath::StringType module_path(region->mapped_file.begin(),
                                             region->mapped_file.end());
      value->SetString("mf",
                       base::FilePath(module_path).BaseName().AsUTF8Unsafe());
    } else {
      value->SetString("mf", region->mapped_file);
    }

    value->BeginDictionary("bs");  // byte stats
    value->SetString(
        "pss",
        base::StringPrintf(kHexFmt, region->byte_stats_proportional_resident));
    value->SetString(
        "pd",
        base::StringPrintf(kHexFmt, region->byte_stats_private_dirty_resident));
    value->SetString(
        "pc",
        base::StringPrintf(kHexFmt, region->byte_stats_private_clean_resident));
    value->SetString(
        "sd",
        base::StringPrintf(kHexFmt, region->byte_stats_shared_dirty_resident));
    value->SetString(
        "sc",
        base::StringPrintf(kHexFmt, region->byte_stats_shared_clean_resident));
    value->SetString("sw",
                     base::StringPrintf(kHexFmt, region->byte_stats_swapped));
    value->EndDictionary();

    value->EndDictionary();
  }
  value->EndArray();
}

bool TracingObserver::IsDumpModeAllowed(
    base::trace_event::MemoryDumpLevelOfDetail dump_mode) const {
  if (!memory_dump_config_)
    return false;
  return memory_dump_config_->allowed_dump_modes.count(dump_mode) != 0;
}

}  // namespace memory_instrumentation
