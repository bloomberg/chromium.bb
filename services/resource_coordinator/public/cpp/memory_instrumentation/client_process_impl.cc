// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/memory_instrumentation/client_process_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/coordinator.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"
#include "services/resource_coordinator/public/interfaces/memory_instrumentation/memory_instrumentation.mojom.h"
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
        service_manager::BindSourceInfo(), mojo::MakeRequest(&coordinator_));
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
}

ClientProcessImpl::~ClientProcessImpl() {}

void ClientProcessImpl::RequestProcessMemoryDump(
    const base::trace_event::MemoryDumpRequestArgs& args,
    const RequestProcessMemoryDumpCallback& callback) {
  base::trace_event::MemoryDumpManager::GetInstance()->CreateProcessDump(
      args, base::Bind(&ClientProcessImpl::OnProcessMemoryDumpDone,
                       base::Unretained(this), callback));
}

void ClientProcessImpl::OnProcessMemoryDumpDone(
    const RequestProcessMemoryDumpCallback& callback,
    bool success,
    uint64_t dump_guid,
    const base::Optional<base::trace_event::MemoryDumpCallbackResult>& result) {
  mojom::RawProcessMemoryDumpPtr process_memory_dump(
      mojom::RawProcessMemoryDump::New());
  if (result) {
    process_memory_dump->os_dump = result->os_dump;
    process_memory_dump->chrome_dump = result->chrome_dump;
    for (const auto& kv : result->extra_processes_dumps) {
      const base::ProcessId pid = kv.first;
      const base::trace_event::MemoryDumpCallbackResult::OSMemDump&
          os_mem_dump = kv.second;
      DCHECK_EQ(0u, process_memory_dump->extra_processes_dumps.count(pid));
      process_memory_dump->extra_processes_dumps[pid] = os_mem_dump;
    }
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
    const std::vector<base::ProcessId>& ids,
    const RequestOSMemoryDumpCallback& callback) {
  using OSMemDump = base::trace_event::MemoryDumpCallbackResult::OSMemDump;
  std::unordered_map<base::ProcessId, OSMemDump> results;
  callback.Run(true, results);
}

ClientProcessImpl::Config::Config(service_manager::Connector* connector,
                                  const std::string& service_name,
                                  mojom::ProcessType process_type)
    : connector(connector),
      service_name(service_name),
      process_type(process_type) {}

ClientProcessImpl::Config::~Config() {}

}  // namespace memory_instrumentation
