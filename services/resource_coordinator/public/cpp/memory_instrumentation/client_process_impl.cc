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
#include "services/resource_coordinator/public/cpp/memory_instrumentation/tracing_observer.h"
#include "services/resource_coordinator/public/interfaces/memory_instrumentation/memory_instrumentation.mojom.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "services/service_manager/public/cpp/connector.h"

namespace memory_instrumentation {

namespace {

mojom::RawOSMemDumpPtr CreateOsDumpFromProcessMemoryDump(
    const base::trace_event::ProcessMemoryDump* pmd) {
  mojom::RawOSMemDumpPtr result = mojom::RawOSMemDump::New();
  if (pmd->has_process_totals()) {
    const base::trace_event::ProcessMemoryTotals* totals =
        pmd->process_totals();
    result->resident_set_kb = totals->resident_set_bytes() / 1024;
    result->platform_private_footprint = totals->GetPlatformPrivateFootprint();
  }
  return result;
}

uint32_t GetDumpsSumKb(const std::string& pattern,
                       const base::trace_event::ProcessMemoryDump* pmd) {
  uint64_t sum = 0;
  for (const auto& kv : pmd->allocator_dumps()) {
    auto name = base::StringPiece(kv.first);
    if (base::MatchPattern(name, pattern)) {
      sum += kv.second->GetSizeInternal();
    }
  }
  return sum / 1024;
}

mojom::RawProcessMemoryDumpPtr CreateDumpSummary(
    const base::trace_event::ProcessMemoryDumpsMap& process_dumps) {
  mojom::RawProcessMemoryDumpPtr result = mojom::RawProcessMemoryDump::New();
  result->chrome_dump = mojom::ChromeMemDump::New();
  for (const auto& kv : process_dumps) {
    base::ProcessId pid = kv.first;  // kNullProcessId for the current process.
    const base::trace_event::ProcessMemoryDump* process_memory_dump =
        kv.second.get();

    // TODO(hjd): Transitional until we send the full PMD. See crbug.com/704203
    if (pid == base::kNullProcessId) {
      result->chrome_dump->malloc_total_kb =
          GetDumpsSumKb("malloc", process_memory_dump);
      result->chrome_dump->v8_total_kb =
          GetDumpsSumKb("v8/*", process_memory_dump);

      result->chrome_dump->command_buffer_total_kb =
          GetDumpsSumKb("gpu/gl/textures/*", process_memory_dump);
      result->chrome_dump->command_buffer_total_kb +=
          GetDumpsSumKb("gpu/gl/buffers/*", process_memory_dump);
      result->chrome_dump->command_buffer_total_kb +=
          GetDumpsSumKb("gpu/gl/renderbuffers/*", process_memory_dump);

      // partition_alloc reports sizes for both allocated_objects and
      // partitions. The memory allocated_objects uses is a subset of
      // the partitions memory so to avoid double counting we only
      // count partitions memory.
      result->chrome_dump->partition_alloc_total_kb =
          GetDumpsSumKb("partition_alloc/partitions/*", process_memory_dump);
      result->chrome_dump->blink_gc_total_kb =
          GetDumpsSumKb("blink_gc", process_memory_dump);
      result->os_dump = CreateOsDumpFromProcessMemoryDump(process_memory_dump);
    } else {
      result->extra_processes_dumps[pid] =
          CreateOsDumpFromProcessMemoryDump(process_memory_dump);
    }
  }
  return result;
}

mojom::RawProcessMemoryDumpPtr CreateEmptyDumpSummary() {
  mojom::RawProcessMemoryDumpPtr result = mojom::RawProcessMemoryDump::New();
  result->chrome_dump = mojom::ChromeMemDump::New();
  result->os_dump = mojom::RawOSMemDump::New();
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

void ClientProcessImpl::RequestProcessMemoryDump(
    const base::trace_event::MemoryDumpRequestArgs& args,
    const RequestProcessMemoryDumpCallback& callback) {
  DCHECK(!callback.is_null());
  base::trace_event::MemoryDumpManager::GetInstance()->CreateProcessDump(
      args, base::Bind(&ClientProcessImpl::OnProcessMemoryDumpDone,
                       base::Unretained(this), callback, args));
}

void ClientProcessImpl::OnProcessMemoryDumpDone(
    const RequestProcessMemoryDumpCallback& callback,
    const base::trace_event::MemoryDumpRequestArgs& req_args,
    bool success,
    uint64_t dump_guid,
    const base::trace_event::ProcessMemoryDumpsMap& process_dumps) {
  DCHECK(success || process_dumps.empty());
  for (const auto& kv : process_dumps) {
    base::ProcessId pid = kv.first;  // kNullProcessId for the current process.
    base::trace_event::ProcessMemoryDump* process_memory_dump = kv.second.get();

    // SUMMARY_ONLY dumps are just return the summarized result in the
    // ProcessMemoryDumpCallback. These shouldn't be added to the trace to
    // avoid confusing trace consumers.
    if (req_args.dump_type != base::trace_event::MemoryDumpType::SUMMARY_ONLY) {
      bool added_to_trace = tracing_observer_->AddDumpToTraceIfEnabled(
          &req_args, pid, process_memory_dump);

      success = success && added_to_trace;
    }
  }

  mojom::RawProcessMemoryDumpPtr process_memory_dump;

  // TODO(hjd): Transitional until we send the full PMD. See crbug.com/704203
  // Don't try to fill the struct in detailed mode since it is hard to avoid
  // double counting.
  if (req_args.level_of_detail ==
      base::trace_event::MemoryDumpLevelOfDetail::DETAILED) {
    process_memory_dump = CreateEmptyDumpSummary();
  } else {
    process_memory_dump = CreateDumpSummary(process_dumps);
  }
  callback.Run(success, dump_guid, std::move(process_memory_dump));
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
    const std::vector<base::ProcessId>& ids,
    const RequestOSMemoryDumpCallback& callback) {
  std::unordered_map<base::ProcessId, mojom::RawOSMemDumpPtr> results;
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
