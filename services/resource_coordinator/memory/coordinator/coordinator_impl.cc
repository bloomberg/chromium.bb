// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/memory/coordinator/coordinator_impl.h"

#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "services/resource_coordinator/public/interfaces/memory/memory_instrumentation.mojom.h"

namespace memory_instrumentation {

namespace {

base::LazyInstance<CoordinatorImpl>::Leaky g_coordinator =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
CoordinatorImpl* CoordinatorImpl::GetInstance() {
  return g_coordinator.Pointer();
}

// TODO(chiniforooshan): Initialize the global MemoryDumpManager instance here.
// This is how the global MemoryDumpManager gets a reference to the delegate on
// the service (read the browser) process for service process memory dumps. This
// can be done when the delegate implementation is landed.
CoordinatorImpl::CoordinatorImpl() : failed_memory_dump_count_(0) {}

CoordinatorImpl::~CoordinatorImpl() {}

void CoordinatorImpl::BindCoordinatorRequest(
    mojom::CoordinatorRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

CoordinatorImpl::QueuedMemoryDumpRequest::QueuedMemoryDumpRequest(
    const base::trace_event::MemoryDumpRequestArgs args,
    const RequestGlobalMemoryDumpCallback callback)
    : args(args), callback(callback) {}

CoordinatorImpl::QueuedMemoryDumpRequest::~QueuedMemoryDumpRequest() {}

void CoordinatorImpl::RequestGlobalMemoryDump(
    const base::trace_event::MemoryDumpRequestArgs& args,
    const RequestGlobalMemoryDumpCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  bool another_dump_already_in_progress = !queued_memory_dump_requests_.empty();

  // If this is a periodic memory dump request and there already is another
  // request in the queue with the same level of detail, there's no point in
  // enqueuing this request.
  if (another_dump_already_in_progress &&
      args.dump_type == base::trace_event::MemoryDumpType::PERIODIC_INTERVAL) {
    for (const auto& request : queued_memory_dump_requests_) {
      if (request.args.level_of_detail == args.level_of_detail) {
        VLOG(1) << base::trace_event::MemoryDumpManager::kLogPrefix << " ("
                << base::trace_event::MemoryDumpTypeToString(args.dump_type)
                << ") skipped because another dump request with the same "
                   "level of detail ("
                << base::trace_event::MemoryDumpLevelOfDetailToString(
                       args.level_of_detail)
                << ") is already in the queue";
        callback.Run(args.dump_guid, false /* success */);
        return;
      }
    }
  }

  queued_memory_dump_requests_.emplace_back(args, callback);

  // If another dump is already in progress, this dump will automatically be
  // scheduled when the other dump finishes.
  if (another_dump_already_in_progress)
    return;

  PerformNextQueuedGlobalMemoryDump();
}

void CoordinatorImpl::RegisterProcessLocalDumpManager(
    mojom::ProcessLocalDumpManagerPtr process_manager) {
  DCHECK(thread_checker_.CalledOnValidThread());

  process_manager.set_connection_error_handler(
      base::Bind(&CoordinatorImpl::UnregisterProcessLocalDumpManager,
                 base::Unretained(this), process_manager.get()));
  auto result = process_managers_.insert(
      std::make_pair<mojom::ProcessLocalDumpManager*,
                     mojom::ProcessLocalDumpManagerPtr>(
          process_manager.get(), std::move(process_manager)));
  DCHECK(result.second);
}

void CoordinatorImpl::UnregisterProcessLocalDumpManager(
    mojom::ProcessLocalDumpManager* process_manager) {
  DCHECK(process_managers_.erase(process_manager) == 1);

  // Check if we are waiting for an ack from this process-local manager.
  if (pending_process_managers_.find(process_manager) !=
      pending_process_managers_.end()) {
    DCHECK(!queued_memory_dump_requests_.empty());
    OnProcessMemoryDumpResponse(
        process_manager, queued_memory_dump_requests_.front().args.dump_guid,
        false /* success */);
  }
}

void CoordinatorImpl::PerformNextQueuedGlobalMemoryDump() {
  DCHECK(!queued_memory_dump_requests_.empty());
  const base::trace_event::MemoryDumpRequestArgs& args =
      queued_memory_dump_requests_.front().args;

  // No need to treat the service process different than other processes. The
  // service process will register itself as a ProcessLocalDumpManager and will
  // be treated like other process-local managers.
  pending_process_managers_.clear();
  failed_memory_dump_count_ = 0;
  for (const auto& key_value : process_managers_) {
    pending_process_managers_.insert(key_value.first);
    key_value.second->RequestProcessMemoryDump(
        args, base::Bind(&CoordinatorImpl::OnProcessMemoryDumpResponse,
                         base::Unretained(this), key_value.first));
  }
  // Run the callback in case there are no process-local managers.
  FinalizeGlobalMemoryDumpIfAllManagersReplied();
}

void CoordinatorImpl::OnProcessMemoryDumpResponse(
    mojom::ProcessLocalDumpManager* process_manager,
    uint64_t dump_guid,
    bool success) {
  auto it = pending_process_managers_.find(process_manager);

  DCHECK(!queued_memory_dump_requests_.empty());
  if (queued_memory_dump_requests_.front().args.dump_guid != dump_guid ||
      it == pending_process_managers_.end()) {
    VLOG(1) << "Received unexpected memory dump response: " << dump_guid;
    return;
  }
  pending_process_managers_.erase(it);

  if (!success) {
    ++failed_memory_dump_count_;
    VLOG(1) << base::trace_event::MemoryDumpManager::kLogPrefix
            << " failed because of NACK from provider";
  }
  FinalizeGlobalMemoryDumpIfAllManagersReplied();
}

void CoordinatorImpl::FinalizeGlobalMemoryDumpIfAllManagersReplied() {
  if (pending_process_managers_.size() > 0)
    return;

  DCHECK(!queued_memory_dump_requests_.empty());
  {
    const auto& callback = queued_memory_dump_requests_.front().callback;
    const bool global_success = failed_memory_dump_count_ == 0;
    callback.Run(queued_memory_dump_requests_.front().args.dump_guid,
                 global_success);
  }
  queued_memory_dump_requests_.pop_front();

  // Schedule the next queued dump (if applicable).
  if (!queued_memory_dump_requests_.empty()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&CoordinatorImpl::PerformNextQueuedGlobalMemoryDump,
                   base::Unretained(this)));
  }
}

}  // namespace memory_instrumentation
