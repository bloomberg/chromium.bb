// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_MEMORY_INSTRUMENTATION_H_
#define SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_MEMORY_INSTRUMENTATION_H_

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_local_storage.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/coordinator.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_export.h"
#include "services/resource_coordinator/public/interfaces/memory_instrumentation/memory_instrumentation.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace memory_instrumentation {

// This a public API for the memory-infra service and allows any thread/process
// to request memory snapshots. This is a convenience wrapper around the
// memory_instrumentation service and hides away the complexity associated with
// having to deal with it (e.g., maintaining service connections, bindings,
// handling timeouts).
class SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_EXPORT MemoryInstrumentation {
 public:
  using MemoryDumpType = base::trace_event::MemoryDumpType;
  using MemoryDumpLevelOfDetail = base::trace_event::MemoryDumpLevelOfDetail;
  using RequestGlobalDumpCallback =
      base::Callback<void(bool success, mojom::GlobalMemoryDumpPtr)>;
  using RequestGlobalDumpAndAppendToTraceCallback =
      base::Callback<void(bool success, uint64_t dump_id)>;

  static void CreateInstance(service_manager::Connector*,
                             const std::string& service_name);
  static MemoryInstrumentation* GetInstance();

  // Requests a global memory dump.
  // Returns asynchronously, via the callback argument:
  //  (true, global_dump) if succeeded;
  //  (false, nullptr) if failed.
  // The callback (if not null), will be posted on the same thread of the
  // RequestGlobalDump() call.
  void RequestGlobalDump(RequestGlobalDumpCallback);

  // Requests a global memory dump and serializes the result into the trace.
  // This requires that both tracing and the memory-infra category have been
  // previousy enabled. Will just gracefully fail otherwise.
  // Returns asynchronously, via the callback argument:
  //  (true, id of the object injected into the trace) if succeeded;
  //  (false, undefined) if failed.
  // The callback (if not null), will be posted on the same thread of the
  // RequestGlobalDumpAndAppendToTrace() call.
  void RequestGlobalDumpAndAppendToTrace(
      MemoryDumpType,
      MemoryDumpLevelOfDetail,
      RequestGlobalDumpAndAppendToTraceCallback);

  // TODO(hjd): Add a RequestGlobalDump() helper, not bound to tracing.
  // here. The mojom interface is about to change soon and right now there is
  // only one client of this API (process_memory_metrics_emitter.cc). Delay this
  // API and change that only once the cleaned up interface is ready.

 private:
  MemoryInstrumentation(service_manager::Connector* connector,
                        const std::string& service_name);
  ~MemoryInstrumentation();

  const mojom::CoordinatorPtr& GetCoordinatorBindingForCurrentThread();
  void BindCoordinatorRequestOnConnectorThread(mojom::CoordinatorRequest);

  service_manager::Connector* const connector_;
  scoped_refptr<base::SingleThreadTaskRunner> connector_task_runner_;
  base::ThreadLocalStorage::Slot tls_coordinator_;
  const std::string service_name_;

  DISALLOW_COPY_AND_ASSIGN(MemoryInstrumentation);
};

}  // namespace memory_instrumentation

#endif  // SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_MEMORY_INSTRUMENTATION_H_
