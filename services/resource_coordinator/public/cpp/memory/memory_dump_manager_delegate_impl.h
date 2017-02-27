// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_MEMORY_DUMP_MANAGER_DELEGATE_IMPL_H_
#define SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_MEMORY_DUMP_MANAGER_DELEGATE_IMPL_H_

#include "base/lazy_instance.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
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
  static MemoryDumpManagerDelegateImpl* GetInstance();

  // Use to bind to a remote coordinator.
  void InitializeWithInterfaceProvider(
      service_manager::InterfaceProvider* interface_provider);

  // Use to bind to a coordinator in the same process.
  void InitializeWithCoordinator(
      Coordinator* coordinator,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  bool IsCoordinator() const override;

  // The base::trace_event::MemoryDumpManager calls this.
  void RequestGlobalMemoryDump(
      const base::trace_event::MemoryDumpRequestArgs& args,
      const base::trace_event::MemoryDumpCallback& callback) override;

  void SetAsNonCoordinatorForTesting();

 private:
  friend std::default_delete<MemoryDumpManagerDelegateImpl>;  // For testing
  friend class MemoryDumpManagerDelegateImplTest;             // For testing
  friend struct base::DefaultLazyInstanceTraits<MemoryDumpManagerDelegateImpl>;

  MemoryDumpManagerDelegateImpl();
  ~MemoryDumpManagerDelegateImpl() override;

  // The ProcessLocalDumpManager interface. The coordinator calls this.
  void RequestProcessMemoryDump(
      const base::trace_event::MemoryDumpRequestArgs& args,
      const RequestProcessMemoryDumpCallback& callback) override;

  // A proxy callback for updating |pending_memory_dump_guid_|.
  void MemoryDumpCallbackProxy(
      const base::trace_event::MemoryDumpCallback& callback,
      uint64_t dump_guid,
      bool success);

  bool initialized_;
  mojom::CoordinatorPtr coordinator_;
  mojo::Binding<mojom::ProcessLocalDumpManager> binding_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  uint64_t pending_memory_dump_guid_;

  // Prevents racy access to |pending_memory_dump_guid_|.
  base::Lock pending_memory_dump_guid_lock_;

  // Prevents racy access to |initialized_|.
  base::Lock initialized_lock_;

  DISALLOW_COPY_AND_ASSIGN(MemoryDumpManagerDelegateImpl);
};

}  // namespace memory_instrumentation

#endif  // SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_MEMORY_DUMP_MANAGER_DELEGATE_IMPL_H_
