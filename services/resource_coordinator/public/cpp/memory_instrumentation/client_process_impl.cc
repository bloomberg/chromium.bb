// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/memory_instrumentation/client_process_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/pattern.h"
#include "base/strings/string_piece.h"
#include "base/synchronization/lock.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/coordinator.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/os_metrics.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/tracing_observer.h"
#include "services/resource_coordinator/public/interfaces/memory_instrumentation/memory_instrumentation.mojom.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "services/service_manager/public/cpp/connector.h"

namespace memory_instrumentation {

namespace {

uint32_t GetDumpsSumKb(const std::string& pattern,
                       const base::trace_event::ProcessMemoryDump& pmd) {
  uint64_t sum = 0;
  for (const auto& kv : pmd.allocator_dumps()) {
    auto name = base::StringPiece(kv.first);
    if (base::MatchPattern(name, pattern)) {
      sum += kv.second->GetSizeInternal();
    }
  }
  return sum / 1024;
}

mojom::ChromeMemDumpPtr CreateDumpSummary(
    const base::trace_event::ProcessMemoryDump& process_memory_dump) {
  mojom::ChromeMemDumpPtr result = mojom::ChromeMemDump::New();
  // TODO(hjd): Transitional until we send the full PMD. See crbug.com/704203
  result->malloc_total_kb = GetDumpsSumKb("malloc", process_memory_dump);
  result->v8_total_kb = GetDumpsSumKb("v8/*", process_memory_dump);

  result->command_buffer_total_kb =
      GetDumpsSumKb("gpu/gl/textures/*", process_memory_dump);
  result->command_buffer_total_kb +=
      GetDumpsSumKb("gpu/gl/buffers/*", process_memory_dump);
  result->command_buffer_total_kb +=
      GetDumpsSumKb("gpu/gl/renderbuffers/*", process_memory_dump);

  // partition_alloc reports sizes for both allocated_objects and
  // partitions. The memory allocated_objects uses is a subset of
  // the partitions memory so to avoid double counting we only
  // count partitions memory.
  result->partition_alloc_total_kb =
      GetDumpsSumKb("partition_alloc/partitions/*", process_memory_dump);
  result->blink_gc_total_kb = GetDumpsSumKb("blink_gc", process_memory_dump);
  return result;
}

}  // namespace

// static
void ClientProcessImpl::CreateInstance(const Config& config) {
  static ClientProcessImpl* instance = nullptr;
  if (!instance) {
    instance = new ClientProcessImpl(config);
  } else {
    NOTREACHED();
  }
}

ClientProcessImpl::ClientProcessImpl(const Config& config)
    : binding_(this), process_type_(config.process_type), task_runner_() {
  // |config.connector| can be null in tests.
  if (config.connector) {
    config.connector->BindInterface(config.service_name,
                                    mojo::MakeRequest(&coordinator_));
    mojom::ClientProcessPtr process;
    binding_.Bind(mojo::MakeRequest(&process));
    coordinator_->RegisterClientProcess(std::move(process),
                                        config.process_type);

    // Initialize the public-facing MemoryInstrumentation helper.
    MemoryInstrumentation::CreateInstance(config.connector,
                                          config.service_name);
  } else {
    config.coordinator_for_testing->BindCoordinatorRequest(
        mojo::MakeRequest(&coordinator_), service_manager::BindSourceInfo());
  }

  task_runner_ = base::ThreadTaskRunnerHandle::Get();

  // TODO(primiano): this is a temporary workaround to tell the
  // base::MemoryDumpManager that it is special and should coordinate periodic
  // dumps for tracing. Remove this once the periodic dump scheduler is moved
  // from base to the service. MDM should not care about being the coordinator.
  bool is_coordinator_process =
      config.process_type == mojom::ProcessType::BROWSER;
  base::trace_event::MemoryDumpManager::GetInstance()->Initialize(
      base::BindRepeating(
          &ClientProcessImpl::RequestGlobalMemoryDump_NoCallback,
          base::Unretained(this)),
      is_coordinator_process);

  tracing_observer_ = base::MakeUnique<TracingObserver>(
      base::trace_event::TraceLog::GetInstance(),
      base::trace_event::MemoryDumpManager::GetInstance());
}

ClientProcessImpl::~ClientProcessImpl() {}

void ClientProcessImpl::RequestChromeMemoryDump(
    const base::trace_event::MemoryDumpRequestArgs& args,
    const RequestChromeMemoryDumpCallback& callback) {
  DCHECK(!callback.is_null());
  base::trace_event::MemoryDumpManager::GetInstance()->CreateProcessDump(
      args, base::Bind(&ClientProcessImpl::OnChromeMemoryDumpDone,
                       base::Unretained(this), callback, args));
}

void ClientProcessImpl::OnChromeMemoryDumpDone(
    const RequestChromeMemoryDumpCallback& callback,
    const base::trace_event::MemoryDumpRequestArgs& req_args,
    bool success,
    uint64_t dump_guid,
    std::unique_ptr<base::trace_event::ProcessMemoryDump> process_memory_dump) {
  DCHECK(success || !process_memory_dump);
  if (!process_memory_dump) {
    callback.Run(false, dump_guid, nullptr);
    return;
  }
  // SUMMARY_ONLY dumps should just return the summarized result in the
  // ProcessMemoryDumpCallback. These shouldn't be added to the trace to
  // avoid confusing trace consumers.
  if (req_args.dump_type != base::trace_event::MemoryDumpType::SUMMARY_ONLY) {
    bool added_to_trace = tracing_observer_->AddChromeDumpToTraceIfEnabled(
        req_args, process_memory_dump.get());
    success = success && added_to_trace;
  }

  mojom::ChromeMemDumpPtr chrome_memory_dump;

  // TODO(hjd): Transitional until we send the full PMD. See crbug.com/704203
  // Don't try to fill the struct in detailed mode since it is hard to avoid
  // double counting.
  if (req_args.level_of_detail ==
      base::trace_event::MemoryDumpLevelOfDetail::DETAILED) {
    chrome_memory_dump = mojom::ChromeMemDump::New();
  } else {
    chrome_memory_dump = CreateDumpSummary(*process_memory_dump.get());
  }
  callback.Run(success, dump_guid, std::move(chrome_memory_dump));
}

void ClientProcessImpl::RequestGlobalMemoryDump_NoCallback(
    const base::trace_event::MemoryDumpRequestArgs& args) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&ClientProcessImpl::RequestGlobalMemoryDump_NoCallback,
                   base::Unretained(this), args));
    return;
  }

  coordinator_->RequestGlobalMemoryDump(
      args, mojom::Coordinator::RequestGlobalMemoryDumpCallback());
}

void ClientProcessImpl::RequestOSMemoryDump(
    bool want_mmaps,
    const std::vector<base::ProcessId>& pids,
    const RequestOSMemoryDumpCallback& callback) {
  std::unordered_map<base::ProcessId, mojom::RawOSMemDumpPtr> results;
  for (const base::ProcessId& pid : pids) {
    mojom::RawOSMemDumpPtr result = mojom::RawOSMemDump::New();
    result->platform_private_footprint = mojom::PlatformPrivateFootprint::New();
    bool success = true;
    success = success && OSMetrics::FillOSMemoryDump(pid, result.get());
    if (want_mmaps)
      success = success && OSMetrics::FillProcessMemoryMaps(pid, result.get());
    if (success)
      results[pid] = std::move(result);
  }
  callback.Run(true, std::move(results));
}

ClientProcessImpl::Config::Config(service_manager::Connector* connector,
                                  const std::string& service_name,
                                  mojom::ProcessType process_type)
    : connector(connector),
      service_name(service_name),
      process_type(process_type) {}

ClientProcessImpl::Config::~Config() {}

}  // namespace memory_instrumentation
