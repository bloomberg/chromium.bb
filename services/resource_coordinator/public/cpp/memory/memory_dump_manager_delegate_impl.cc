// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/memory/memory_dump_manager_delegate_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/resource_coordinator/public/cpp/memory/coordinator.h"
#include "services/resource_coordinator/public/interfaces/memory/memory_instrumentation.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace memory_instrumentation {

MemoryDumpManagerDelegateImpl::Config::~Config() {}

MemoryDumpManagerDelegateImpl::MemoryDumpManagerDelegateImpl(
    const MemoryDumpManagerDelegateImpl::Config& config)
    : binding_(this),
      config_(config),
      task_runner_(nullptr),
      pending_memory_dump_guid_(0) {
  if (config.connector() != nullptr) {
    config.connector()->BindInterface(config.service_name(),
                                      mojo::MakeRequest(&coordinator_));
  } else {
    task_runner_ = base::ThreadTaskRunnerHandle::Get();
    config.coordinator()->BindCoordinatorRequest(
        mojo::MakeRequest(&coordinator_));
  }
  coordinator_->RegisterProcessLocalDumpManager(
      binding_.CreateInterfacePtrAndBind());
}

MemoryDumpManagerDelegateImpl::~MemoryDumpManagerDelegateImpl() {}

bool MemoryDumpManagerDelegateImpl::IsCoordinator() const {
  return task_runner_ != nullptr;
}

void MemoryDumpManagerDelegateImpl::RequestProcessMemoryDump(
    const base::trace_event::MemoryDumpRequestArgs& args,
    const RequestProcessMemoryDumpCallback& callback) {
  MemoryDumpManagerDelegate::CreateProcessDump(args, callback);
}

void MemoryDumpManagerDelegateImpl::RequestGlobalMemoryDump(
    const base::trace_event::MemoryDumpRequestArgs& args,
    const base::trace_event::MemoryDumpCallback& callback) {
  // Note: This condition is here to match the old behavior. If the delegate is
  // in the browser process, we do not drop parallel requests in the delegate
  // and so they will be queued by the Coordinator service (see
  // CoordinatorImpl::RequestGlobalMemoryDump). If the delegate is in a child
  // process, parallel requests will be cancelled.
  //
  // TODO(chiniforooshan): Unify the child and browser behavior.
  if (IsCoordinator()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&mojom::Coordinator::RequestGlobalMemoryDump,
                   base::Unretained(coordinator_.get()), args, callback));
    return;
  }

  {
    base::AutoLock lock(pending_memory_dump_guid_lock_);
    if (pending_memory_dump_guid_) {
      callback.Run(args.dump_guid, false);
      return;
    }
    pending_memory_dump_guid_ = args.dump_guid;
  }
  auto callback_proxy =
      base::Bind(&MemoryDumpManagerDelegateImpl::MemoryDumpCallbackProxy,
                 base::Unretained(this), callback);
  coordinator_->RequestGlobalMemoryDump(args, callback_proxy);
}

void MemoryDumpManagerDelegateImpl::MemoryDumpCallbackProxy(
    const base::trace_event::MemoryDumpCallback& callback,
    uint64_t dump_guid,
    bool success) {
  DCHECK_NE(0U, pending_memory_dump_guid_);
  pending_memory_dump_guid_ = 0;
  callback.Run(dump_guid, success);
}

void MemoryDumpManagerDelegateImpl::SetAsNonCoordinatorForTesting() {
  task_runner_ = nullptr;
}

}  // namespace memory_instrumentation
