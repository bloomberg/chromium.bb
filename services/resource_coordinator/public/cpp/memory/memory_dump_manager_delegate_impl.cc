// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/memory/memory_dump_manager_delegate_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/resource_coordinator/public/cpp/memory/coordinator.h"
#include "services/resource_coordinator/public/interfaces/memory/memory_instrumentation.mojom.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace memory_instrumentation {

namespace {

base::LazyInstance<MemoryDumpManagerDelegateImpl>::Leaky
    g_memory_dump_manager_delegate = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
MemoryDumpManagerDelegateImpl* MemoryDumpManagerDelegateImpl::GetInstance() {
  return g_memory_dump_manager_delegate.Pointer();
}

MemoryDumpManagerDelegateImpl::MemoryDumpManagerDelegateImpl()
    : initialized_(false),
      binding_(this),
      task_runner_(nullptr),
      pending_memory_dump_guid_(0) {}

MemoryDumpManagerDelegateImpl::~MemoryDumpManagerDelegateImpl() {}

void MemoryDumpManagerDelegateImpl::InitializeWithInterfaceProvider(
    service_manager::InterfaceProvider* interface_provider) {
  {
    base::AutoLock lock(initialized_lock_);
    DCHECK(!initialized_);
    initialized_ = true;
  }

  interface_provider->GetInterface(mojo::MakeRequest(&coordinator_));
  coordinator_->RegisterProcessLocalDumpManager(
      binding_.CreateInterfacePtrAndBind());
  base::trace_event::MemoryDumpManager::GetInstance()->Initialize(this);
}

void MemoryDumpManagerDelegateImpl::InitializeWithCoordinator(
    Coordinator* coordinator,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(task_runner);
  if (!task_runner->RunsTasksOnCurrentThread()) {
    task_runner->PostTask(
        FROM_HERE,
        base::Bind(&MemoryDumpManagerDelegateImpl::InitializeWithCoordinator,
                   base::Unretained(this), base::Unretained(coordinator),
                   task_runner));
    return;
  }

  {
    base::AutoLock lock(initialized_lock_);
    DCHECK(!initialized_);
    initialized_ = true;
  }

  task_runner_ = task_runner;
  coordinator->BindCoordinatorRequest(mojo::MakeRequest(&coordinator_));
  coordinator_->RegisterProcessLocalDumpManager(
      binding_.CreateInterfacePtrAndBind());
  base::trace_event::MemoryDumpManager::GetInstance()->Initialize(this);
}

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
  // TODO(chiniforooshan): After transitioning to the mojo-based service is
  // completed, we should enable queueing parallel global memory dump requests
  // by delegates on all processes.
  if (task_runner_) {
    DCHECK(task_runner_);
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
  DCHECK(!task_runner_);
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
