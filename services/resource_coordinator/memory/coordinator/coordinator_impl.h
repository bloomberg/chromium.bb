// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_MEMORY_COORDINATOR_COORDINATOR_IMPL_H_
#define SERVICES_RESOURCE_COORDINATOR_MEMORY_COORDINATOR_COORDINATOR_IMPL_H_

#include <list>
#include <set>
#include <unordered_map>

#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/resource_coordinator/public/cpp/memory/coordinator.h"
#include "services/resource_coordinator/public/interfaces/memory/memory_instrumentation.mojom.h"

namespace memory_instrumentation {

class CoordinatorImpl : public Coordinator, public mojom::Coordinator {
 public:
  static CoordinatorImpl* GetInstance();

  // Coordinator
  void BindCoordinatorRequest(mojom::CoordinatorRequest) override;

 private:
  friend class CoordinatorImplTest;  // For testing
  friend struct base::DefaultLazyInstanceTraits<CoordinatorImpl>;

  struct QueuedMemoryDumpRequest {
    QueuedMemoryDumpRequest(const base::trace_event::MemoryDumpRequestArgs args,
                            const RequestGlobalMemoryDumpCallback callback);
    ~QueuedMemoryDumpRequest();
    const base::trace_event::MemoryDumpRequestArgs args;
    const RequestGlobalMemoryDumpCallback callback;
  };

  CoordinatorImpl();
  ~CoordinatorImpl() override;

  // mojom::Coordinator
  void RegisterProcessLocalDumpManager(
      mojom::ProcessLocalDumpManagerPtr process_manager) override;

  // Broadcasts a dump request to all the process-local managers registered and
  // notifies when all of them have completed, or the global dump attempt
  // failed. This is in the mojom::Coordinator interface.
  void RequestGlobalMemoryDump(
      const base::trace_event::MemoryDumpRequestArgs& args,
      const RequestGlobalMemoryDumpCallback& callback) override;

  // Called when a process-local manager gets disconnected.
  void UnregisterProcessLocalDumpManager(
      mojom::ProcessLocalDumpManager* process_manager);

  // Callback of RequestProcessMemoryDump.
  void OnProcessMemoryDumpResponse(
      mojom::ProcessLocalDumpManager* process_manager,
      uint64_t dump_guid,
      bool success);

  void PerformNextQueuedGlobalMemoryDump();
  void FinalizeGlobalMemoryDumpIfAllManagersReplied();

  mojo::BindingSet<mojom::Coordinator> bindings_;

  // Registered ProcessLocalDumpManagers.
  std::unordered_map<mojom::ProcessLocalDumpManager*,
                     mojom::ProcessLocalDumpManagerPtr>
      process_managers_;

  // Pending process managers for RequestGlobalMemoryDump.
  std::set<mojom::ProcessLocalDumpManager*> pending_process_managers_;
  int failed_memory_dump_count_;
  std::list<QueuedMemoryDumpRequest> queued_memory_dump_requests_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(CoordinatorImpl);
};

}  // namespace memory_instrumentation
#endif  // SERVICES_RESOURCE_COORDINATOR_MEMORY_COORDINATOR_COORDINATOR_IMPL_H_
