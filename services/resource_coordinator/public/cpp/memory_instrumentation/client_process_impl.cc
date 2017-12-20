// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/memory_instrumentation/client_process_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/single_thread_task_runner.h"
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

  tracing_observer_ = std::make_unique<TracingObserver>(
      base::trace_event::TraceLog::GetInstance(),
      base::trace_event::MemoryDumpManager::GetInstance());
}

ClientProcessImpl::~ClientProcessImpl() {}

void ClientProcessImpl::RequestChromeMemoryDump(
    const base::trace_event::MemoryDumpRequestArgs& args,
    const RequestChromeMemoryDumpCallback& callback) {
  DCHECK(!callback.is_null());
  auto it_and_inserted =
      pending_chrome_callbacks_.emplace(args.dump_guid, std::move(callback));
  DCHECK(it_and_inserted.second) << "Duplicated request id " << args.dump_guid;
  base::trace_event::MemoryDumpManager::GetInstance()->CreateProcessDump(
      args, base::Bind(&ClientProcessImpl::OnChromeMemoryDumpDone,
                       base::Unretained(this)));
}

void ClientProcessImpl::OnChromeMemoryDumpDone(
    bool success,
    uint64_t dump_guid,
    std::unique_ptr<base::trace_event::ProcessMemoryDump> process_memory_dump) {
  DCHECK(success || !process_memory_dump);

  auto callback_it = pending_chrome_callbacks_.find(dump_guid);
  DCHECK(callback_it != pending_chrome_callbacks_.end());

  auto callback = std::move(callback_it->second);
  pending_chrome_callbacks_.erase(callback_it);

  if (!process_memory_dump) {
    callback.Run(false, dump_guid, nullptr);
    return;
  }
  callback.Run(success, dump_guid, std::move(process_memory_dump));
}

void ClientProcessImpl::RequestGlobalMemoryDump_NoCallback(
    base::trace_event::MemoryDumpType dump_type,
    base::trace_event::MemoryDumpLevelOfDetail level_of_detail) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&ClientProcessImpl::RequestGlobalMemoryDump_NoCallback,
                   base::Unretained(this), dump_type, level_of_detail));
    return;
  }

  coordinator_->RequestGlobalMemoryDumpAndAppendToTrace(
      dump_type, level_of_detail,
      mojom::Coordinator::RequestGlobalMemoryDumpAndAppendToTraceCallback());
}

void ClientProcessImpl::EnableHeapProfiling(
    base::trace_event::HeapProfilingMode mode,
    const EnableHeapProfilingCallback& callback) {
  callback.Run(base::trace_event::MemoryDumpManager::GetInstance()->
                   EnableHeapProfiling(mode));
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
