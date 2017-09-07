// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_MEMORY_INSTRUMENTATION_COORDINATOR_IMPL_H_
#define SERVICES_RESOURCE_COORDINATOR_MEMORY_INSTRUMENTATION_COORDINATOR_IMPL_H_

#include <list>
#include <map>
#include <set>
#include <unordered_map>

#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/resource_coordinator/memory_instrumentation/process_map.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/coordinator.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/tracing_observer.h"
#include "services/resource_coordinator/public/interfaces/memory_instrumentation/memory_instrumentation.mojom.h"

namespace service_manager {
class Connector;
}

namespace memory_instrumentation {

// Memory instrumentation service. It serves two purposes:
// - Handles a registry of the processes that have a memory instrumentation
//   client library instance (../../public/cpp/memory).
// - Provides global (i.e. for all processes) memory snapshots on demand.
//   Global snapshots are obtained by requesting in-process snapshots from each
//   registered client and aggregating them.
class CoordinatorImpl : public Coordinator, public mojom::Coordinator {
 public:
  // The getter of the unique instance.
  static CoordinatorImpl* GetInstance();

  CoordinatorImpl(service_manager::Connector* connector);

  // Binds a client library to this coordinator instance.
  void BindCoordinatorRequest(
      mojom::CoordinatorRequest,
      const service_manager::BindSourceInfo& source_info) override;

  // mojom::Coordinator implementation.
  void RegisterClientProcess(mojom::ClientProcessPtr,
                             mojom::ProcessType) override;
  void UnregisterClientProcess(mojom::ClientProcess*);
  void RequestGlobalMemoryDump(const base::trace_event::MemoryDumpRequestArgs&,
                               const RequestGlobalMemoryDumpCallback&) override;
  void GetVmRegionsForHeapProfiler(
      const GetVmRegionsForHeapProfilerCallback&) override;

 protected:
  // virtual for testing.
  virtual service_manager::Identity GetClientIdentityForCurrentRequest() const;
  // virtual for testing.
  virtual base::ProcessId GetProcessIdForClientIdentity(
      service_manager::Identity identity) const;
  ~CoordinatorImpl() override;

 private:
  using OSMemDumpMap =
      std::unordered_map<base::ProcessId, mojom::RawOSMemDumpPtr>;
  friend std::default_delete<CoordinatorImpl>;  // For testing
  friend class CoordinatorImplTest;             // For testing

  // Holds data for pending requests enqueued via RequestGlobalMemoryDump().
  struct QueuedMemoryDumpRequest {
    QueuedMemoryDumpRequest(
        const base::trace_event::MemoryDumpRequestArgs& args,
        const RequestGlobalMemoryDumpCallback callback);
    ~QueuedMemoryDumpRequest();
    const base::trace_event::MemoryDumpRequestArgs args;
    const RequestGlobalMemoryDumpCallback callback;

    struct PendingResponse {
      enum Type {
        kChromeDump,
        kOSDump,
      };
      PendingResponse(const mojom::ClientProcess* client, const Type type);

      bool operator<(const PendingResponse& other) const;

      const mojom::ClientProcess* client;
      const Type type;
    };

    struct Response {
      Response();
      ~Response();

      base::ProcessId process_id;
      mojom::ProcessType process_type;
      mojom::ChromeMemDumpPtr chrome_dump_ptr;
      OSMemDumpMap os_dumps;
    };

    // When a dump, requested via RequestGlobalMemoryDump(), is in progress this
    // set contains a |PendingResponse| for each |RequestChromeMemoryDump| and
    // |RequestOSMemoryDump| call that has not yet replied or been canceled (due
    // to the client disconnecting).
    std::set<QueuedMemoryDumpRequest::PendingResponse> pending_responses;
    std::map<mojom::ClientProcess*, Response> responses;
    int failed_memory_dump_count = 0;
    bool dump_in_progress = false;
    // The time we started handling the request (does not including queuing
    // time).
    base::Time start_time;
  };

  // Holds the identity and remote reference of registered clients.
  struct ClientInfo {
    ClientInfo(const service_manager::Identity&,
               mojom::ClientProcessPtr,
               mojom::ProcessType);
    ~ClientInfo();

    const service_manager::Identity identity;
    const mojom::ClientProcessPtr client;
    const mojom::ProcessType process_type;
  };

  // Callback of RequestChromeMemoryDump.
  void OnChromeMemoryDumpResponse(mojom::ClientProcess*,
                                  bool success,
                                  uint64_t dump_guid,
                                  mojom::ChromeMemDumpPtr chrome_memory_dump);

  // Callback of RequestOSMemoryDump.
  void OnOSMemoryDumpResponse(mojom::ClientProcess*,
                              bool success,
                              OSMemDumpMap);

  void RemovePendingResponse(mojom::ClientProcess*,
                             QueuedMemoryDumpRequest::PendingResponse::Type);

  void PerformNextQueuedGlobalMemoryDump();
  void FinalizeGlobalMemoryDumpIfAllManagersReplied();
  QueuedMemoryDumpRequest* GetCurrentRequest();

  mojo::BindingSet<mojom::Coordinator, service_manager::Identity> bindings_;

  // Map of registered client processes.
  std::map<mojom::ClientProcess*, std::unique_ptr<ClientInfo>> clients_;

  // Outstanding dump requests, enqueued via RequestGlobalMemoryDump().
  std::list<QueuedMemoryDumpRequest> queued_memory_dump_requests_;

  // Maintains a map of service_manager::Identity -> pid for registered clients.
  std::unique_ptr<ProcessMap> process_map_;
  uint64_t next_dump_id_;
  std::unique_ptr<TracingObserver> tracing_observer_;

  THREAD_CHECKER(thread_checker_);
  DISALLOW_COPY_AND_ASSIGN(CoordinatorImpl);
};

}  // namespace memory_instrumentation
#endif  // SERVICES_RESOURCE_COORDINATOR_MEMORY_INSTRUMENTATION_COORDINATOR_IMPL_H_
