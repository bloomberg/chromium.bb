// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_MEMORY_DUMP_MANAGER_DELEGATE_IMPL_H_
#define SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_MEMORY_DUMP_MANAGER_DELEGATE_IMPL_H_

#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/resource_coordinator/public/cpp/memory/coordinator.h"
#include "services/resource_coordinator/public/interfaces/memory/memory_instrumentation.mojom.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace memory_instrumentation {

class MemoryDumpManagerDelegateImpl
    : public base::trace_event::MemoryDumpManagerDelegate,
      public mojom::ProcessLocalDumpManager {
 public:
  // Use to bind to a remote coordinator.
  MemoryDumpManagerDelegateImpl(
      service_manager::InterfaceProvider* interface_provider);

  // Use to bind to a coordinator in the same process.
  MemoryDumpManagerDelegateImpl(Coordinator* coordinator);
  ~MemoryDumpManagerDelegateImpl() override;

  bool IsCoordinator() const;

  // The base::trace_event::MemoryDumpManager calls this.
  void RequestGlobalMemoryDump(
      const base::trace_event::MemoryDumpRequestArgs& args,
      const base::trace_event::MemoryDumpCallback& callback) override;

 private:
  // The ProcessLocalDumpManager interface. The coordinator calls this.
  void RequestProcessMemoryDump(
      const base::trace_event::MemoryDumpRequestArgs& args,
      const RequestProcessMemoryDumpCallback& callback) override;

  bool is_coordinator_;
  mojom::CoordinatorPtr coordinator_;
  mojo::Binding<mojom::ProcessLocalDumpManager> binding_;

  DISALLOW_COPY_AND_ASSIGN(MemoryDumpManagerDelegateImpl);
};

}  // namespace memory_instrumentation

#endif  // SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_MEMORY_DUMP_MANAGER_DELEGATE_IMPL_H_
