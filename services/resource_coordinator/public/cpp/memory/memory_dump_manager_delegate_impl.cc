// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/memory/memory_dump_manager_delegate_impl.h"

#include "base/trace_event/memory_dump_request_args.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/resource_coordinator/public/cpp/memory/coordinator.h"
#include "services/resource_coordinator/public/interfaces/memory/memory_instrumentation.mojom.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace memory_instrumentation {

MemoryDumpManagerDelegateImpl::MemoryDumpManagerDelegateImpl(
    service_manager::InterfaceProvider* interface_provider)
    : is_coordinator_(false), binding_(this) {
  interface_provider->GetInterface(mojo::MakeRequest(&coordinator_));
  coordinator_->RegisterProcessLocalDumpManager(
      binding_.CreateInterfacePtrAndBind());
}

MemoryDumpManagerDelegateImpl::MemoryDumpManagerDelegateImpl(
    Coordinator* coordinator)
    : is_coordinator_(true), binding_(this) {
  coordinator->BindCoordinatorRequest(mojo::MakeRequest(&coordinator_));
  coordinator_->RegisterProcessLocalDumpManager(
      binding_.CreateInterfacePtrAndBind());
}

MemoryDumpManagerDelegateImpl::~MemoryDumpManagerDelegateImpl() {}

bool MemoryDumpManagerDelegateImpl::IsCoordinator() const {
  return is_coordinator_;
}

void MemoryDumpManagerDelegateImpl::RequestProcessMemoryDump(
    const base::trace_event::MemoryDumpRequestArgs& args,
    const RequestProcessMemoryDumpCallback& callback) {
  MemoryDumpManagerDelegate::CreateProcessDump(args, callback);
}

void MemoryDumpManagerDelegateImpl::RequestGlobalMemoryDump(
    const base::trace_event::MemoryDumpRequestArgs& args,
    const base::trace_event::MemoryDumpCallback& callback) {
  coordinator_->RequestGlobalMemoryDump(args, callback);
}

}  // namespace memory_instrumentation
