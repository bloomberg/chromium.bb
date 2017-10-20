// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/memory_instrumentation/coordinator_impl.h"

#include <inttypes.h>
#include <stdio.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "services/resource_coordinator/memory_instrumentation/queued_request_dispatcher.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/client_process_impl.h"
#include "services/resource_coordinator/public/interfaces/memory_instrumentation/constants.mojom.h"
#include "services/resource_coordinator/public/interfaces/memory_instrumentation/memory_instrumentation.mojom.h"
#include "services/service_manager/public/cpp/identity.h"

#if defined(OS_MACOSX) && !defined(OS_IOS)
#include "base/mac/mac_util.h"
#endif

using base::trace_event::MemoryDumpLevelOfDetail;
using base::trace_event::MemoryDumpType;

namespace memory_instrumentation {

namespace {

memory_instrumentation::CoordinatorImpl* g_coordinator_impl;

}  // namespace


// static
CoordinatorImpl* CoordinatorImpl::GetInstance() {
  return g_coordinator_impl;
}

CoordinatorImpl::CoordinatorImpl(service_manager::Connector* connector)
    : next_dump_id_(0) {
  process_map_ = base::MakeUnique<ProcessMap>(connector);
  DCHECK(!g_coordinator_impl);
  g_coordinator_impl = this;
  base::trace_event::MemoryDumpManager::GetInstance()->set_tracing_process_id(
      mojom::kServiceTracingProcessId);

  tracing_observer_ = base::MakeUnique<TracingObserver>(
      base::trace_event::TraceLog::GetInstance(), nullptr);
}

CoordinatorImpl::~CoordinatorImpl() {
  g_coordinator_impl = nullptr;
}

base::ProcessId CoordinatorImpl::GetProcessIdForClientIdentity(
    service_manager::Identity identity) const {
  DCHECK(identity.IsValid());
  return process_map_->GetProcessId(identity);
}

service_manager::Identity CoordinatorImpl::GetClientIdentityForCurrentRequest()
    const {
  return bindings_.dispatch_context();
}

void CoordinatorImpl::BindCoordinatorRequest(
    mojom::CoordinatorRequest request,
    const service_manager::BindSourceInfo& source_info) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  bindings_.AddBinding(this, std::move(request), source_info.identity);
}

void CoordinatorImpl::RequestGlobalMemoryDump(
    const base::trace_event::MemoryDumpRequestArgs& args_in,
    const RequestGlobalMemoryDumpCallback& callback) {
  // This merely strips out the |dump_guid| argument.
  auto callback_adapter = [](const RequestGlobalMemoryDumpCallback& callback,
                             bool success, uint64_t,
                             mojom::GlobalMemoryDumpPtr global_memory_dump) {
    callback.Run(success, std::move(global_memory_dump));
  };
  RequestGlobalMemoryDumpInternal(args_in, false,
                                  base::Bind(callback_adapter, callback));
}

void CoordinatorImpl::RequestGlobalMemoryDumpAndAppendToTrace(
    const base::trace_event::MemoryDumpRequestArgs& args_in,
    const RequestGlobalMemoryDumpAndAppendToTraceCallback& callback) {
  // This merely strips out the |dump_ptr| argument.
  auto callback_adapter =
      [](const RequestGlobalMemoryDumpAndAppendToTraceCallback& callback,
         bool success, uint64_t dump_guid,
         mojom::GlobalMemoryDumpPtr) { callback.Run(success, dump_guid); };
  RequestGlobalMemoryDumpInternal(args_in, true,
                                  base::Bind(callback_adapter, callback));
}

void CoordinatorImpl::GetVmRegionsForHeapProfiler(
    const GetVmRegionsForHeapProfilerCallback& callback) {
  base::trace_event::MemoryDumpRequestArgs args{
      0 /* dump_guid */, base::trace_event::MemoryDumpType::VM_REGIONS_ONLY,
      base::trace_event::MemoryDumpLevelOfDetail::DETAILED};
  RequestGlobalMemoryDump(args, callback);
}

void CoordinatorImpl::RegisterClientProcess(
    mojom::ClientProcessPtr client_process_ptr,
    mojom::ProcessType process_type) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  mojom::ClientProcess* client_process = client_process_ptr.get();
  client_process_ptr.set_connection_error_handler(
      base::Bind(&CoordinatorImpl::UnregisterClientProcess,
                 base::Unretained(this), client_process));
  auto identity = GetClientIdentityForCurrentRequest();
  auto client_info = base::MakeUnique<ClientInfo>(
      std::move(identity), std::move(client_process_ptr), process_type);
  auto iterator_and_inserted =
      clients_.emplace(client_process, std::move(client_info));
  DCHECK(iterator_and_inserted.second);
}

void CoordinatorImpl::UnregisterClientProcess(
    mojom::ClientProcess* client_process) {
  QueuedRequest* request = GetCurrentRequest();
  if (request != nullptr) {
    // Check if we are waiting for an ack from this client process.
    auto it = request->pending_responses.begin();
    while (it != request->pending_responses.end()) {
      // The calls to On*MemoryDumpResponse below, if executed, will delete the
      // element under the iterator which invalidates it. To avoid this we
      // increment the iterator in advance while keeping a reference to the
      // current element.
      std::set<QueuedRequest::PendingResponse>::iterator current = it++;
      if (current->client != client_process)
        continue;
      RemovePendingResponse(client_process, current->type);
      request->failed_memory_dump_count++;
    }
    FinalizeGlobalMemoryDumpIfAllManagersReplied();
  }
  size_t num_deleted = clients_.erase(client_process);
  DCHECK(num_deleted == 1);
}

void CoordinatorImpl::RequestGlobalMemoryDumpInternal(
    const base::trace_event::MemoryDumpRequestArgs& args_in,
    bool add_to_trace,
    const RequestGlobalMemoryDumpInternalCallback& callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  UMA_HISTOGRAM_COUNTS_1000("Memory.Experimental.Debug.GlobalDumpQueueLength",
                            queued_memory_dump_requests_.size());

  bool another_dump_is_queued = !queued_memory_dump_requests_.empty();

  // TODO(primiano): remove dump_guid from the request. For the moment callers
  // should just pass a zero |dump_guid| in input. It should be an out-only arg.
  DCHECK_EQ(0u, args_in.dump_guid);
  base::trace_event::MemoryDumpRequestArgs args = args_in;
  args.dump_guid = ++next_dump_id_;

  // If this is a periodic or peak memory dump request and there already is
  // another request in the queue with the same level of detail, there's no
  // point in enqueuing this request.
  if (another_dump_is_queued &&
      args.dump_type !=
          base::trace_event::MemoryDumpType::EXPLICITLY_TRIGGERED &&
      args.dump_type != base::trace_event::MemoryDumpType::SUMMARY_ONLY &&
      args.dump_type != base::trace_event::MemoryDumpType::VM_REGIONS_ONLY) {
    for (const auto& request : queued_memory_dump_requests_) {
      if (request.args.level_of_detail == args.level_of_detail) {
        VLOG(1) << "RequestGlobalMemoryDump("
                << base::trace_event::MemoryDumpTypeToString(args.dump_type)
                << ") skipped because another dump request with the same "
                   "level of detail ("
                << base::trace_event::MemoryDumpLevelOfDetailToString(
                       args.level_of_detail)
                << ") is already in the queue";
        callback.Run(false /* success */, 0 /* dump_guid */,
                     nullptr /* global_memory_dump */);
        return;
      }
    }
  }

  queued_memory_dump_requests_.emplace_back(args, callback, add_to_trace);

  // If another dump is already in queued, this dump will automatically be
  // scheduled when the other dump finishes.
  if (another_dump_is_queued)
    return;

  PerformNextQueuedGlobalMemoryDump();
}

void CoordinatorImpl::PerformNextQueuedGlobalMemoryDump() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  QueuedRequest* request = GetCurrentRequest();

  std::vector<QueuedRequestDispatcher::ClientInfo> clients;
  for (const auto& kv : clients_) {
    auto client_identity = kv.second->identity;
    const base::ProcessId pid = GetProcessIdForClientIdentity(client_identity);
    if (pid == base::kNullProcessId) {
      VLOG(1) << "Couldn't find a PID for client \"" << client_identity.name()
              << "." << client_identity.instance() << "\"";
      continue;
    }
    clients.emplace_back(kv.second->client.get(), pid, kv.second->process_type);
  }

  auto chrome_callback = base::Bind(
      &CoordinatorImpl::OnChromeMemoryDumpResponse, base::Unretained(this));
  auto os_callback = base::Bind(&CoordinatorImpl::OnOSMemoryDumpResponse,
                                base::Unretained(this));
  QueuedRequestDispatcher::SetUpAndDispatch(request, clients, chrome_callback,
                                            os_callback);

  // Run the callback in case there are no client processes registered.
  FinalizeGlobalMemoryDumpIfAllManagersReplied();
}

QueuedRequest* CoordinatorImpl::GetCurrentRequest() {
  if (queued_memory_dump_requests_.empty()) {
    return nullptr;
  }
  return &queued_memory_dump_requests_.front();
}

void CoordinatorImpl::OnChromeMemoryDumpResponse(
    mojom::ClientProcess* client,
    bool success,
    uint64_t dump_guid,
    std::unique_ptr<base::trace_event::ProcessMemoryDump> chrome_memory_dump) {
  using ResponseType = QueuedRequest::PendingResponse::Type;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  QueuedRequest* request = GetCurrentRequest();
  if (request == nullptr) {
    NOTREACHED() << "No current dump request.";
    return;
  }

  RemovePendingResponse(client, ResponseType::kChromeDump);

  if (!clients_.count(client)) {
    VLOG(1) << "Received a memory dump response from an unregistered client";
    return;
  }

  // Only add to trace if requested.
  auto* response = &request->responses[client];
  if (chrome_memory_dump && request->add_to_trace) {
    bool added_to_trace = tracing_observer_->AddChromeDumpToTraceIfEnabled(
        request->args, response->process_id, chrome_memory_dump.get());
    success = success && added_to_trace;
  }
  response->chrome_dump = std::move(chrome_memory_dump);

  if (!success) {
    request->failed_memory_dump_count++;
    VLOG(1) << "RequestGlobalMemoryDump() FAIL: NACK from client process";
  }

  FinalizeGlobalMemoryDumpIfAllManagersReplied();
}

void CoordinatorImpl::OnOSMemoryDumpResponse(mojom::ClientProcess* client,
                                             bool success,
                                             OSMemDumpMap os_dumps) {
  using ResponseType = QueuedRequest::PendingResponse::Type;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  QueuedRequest* request = GetCurrentRequest();
  if (request == nullptr) {
    NOTREACHED() << "No current dump request.";
    return;
  }

  RemovePendingResponse(client, ResponseType::kOSDump);

  if (!clients_.count(client)) {
    VLOG(1) << "Received a memory dump response from an unregistered client";
    return;
  }

  request->responses[client].os_dumps = std::move(os_dumps);

  if (!success) {
    request->failed_memory_dump_count++;
    VLOG(1) << "RequestGlobalMemoryDump() FAIL: NACK from client process";
  }

  FinalizeGlobalMemoryDumpIfAllManagersReplied();
}

void CoordinatorImpl::RemovePendingResponse(
    mojom::ClientProcess* client,
    QueuedRequest::PendingResponse::Type type) {
  QueuedRequest* request = GetCurrentRequest();
  if (request == nullptr) {
    NOTREACHED() << "No current dump request.";
    return;
  }
  auto it = request->pending_responses.find({client, type});
  if (it == request->pending_responses.end()) {
    VLOG(1) << "Unexpected memory dump response";
    return;
  }
  request->pending_responses.erase(it);
}

void CoordinatorImpl::FinalizeGlobalMemoryDumpIfAllManagersReplied() {
  TRACE_EVENT0(base::trace_event::MemoryDumpManager::kTraceCategory,
               "GlobalMemoryDump.Computation");
  DCHECK(!queued_memory_dump_requests_.empty());

  QueuedRequest* request = &queued_memory_dump_requests_.front();
  if (!request->dump_in_progress || request->pending_responses.size() > 0)
    return;

  QueuedRequestDispatcher::Finalize(request, tracing_observer_.get());

  queued_memory_dump_requests_.pop_front();
  request = nullptr;

  // Schedule the next queued dump (if applicable).
  if (!queued_memory_dump_requests_.empty()) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&CoordinatorImpl::PerformNextQueuedGlobalMemoryDump,
                   base::Unretained(this)));
  }
}

CoordinatorImpl::ClientInfo::ClientInfo(
    const service_manager::Identity& identity,
    mojom::ClientProcessPtr client,
    mojom::ProcessType process_type)
    : identity(identity),
      client(std::move(client)),
      process_type(process_type) {}
CoordinatorImpl::ClientInfo::~ClientInfo() {}

}  // namespace memory_instrumentation
