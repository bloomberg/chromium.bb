// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_MEMORY_INSTRUMENTATION_QUEUED_REQUEST_DISPATCHER_H_
#define SERVICES_RESOURCE_COORDINATOR_MEMORY_INSTRUMENTATION_QUEUED_REQUEST_DISPATCHER_H_

#include <map>
#include <memory>
#include <unordered_map>

#include "base/trace_event/memory_dump_request_args.h"
#include "services/resource_coordinator/memory_instrumentation/coordinator_impl.h"
#include "services/resource_coordinator/memory_instrumentation/queued_request.h"

namespace memory_instrumentation {

class QueuedRequestDispatcher {
 public:
  using OSMemDumpMap =
      std::unordered_map<base::ProcessId,
                         memory_instrumentation::mojom::RawOSMemDumpPtr>;
  using RequestGlobalMemoryDumpInternalCallback = base::Callback<
      void(bool, uint64_t, memory_instrumentation::mojom::GlobalMemoryDumpPtr)>;
  using ChromeCallback = base::Callback<void(
      mojom::ClientProcess*,
      bool,
      uint64_t,
      std::unique_ptr<base::trace_event::ProcessMemoryDump>)>;
  using OsCallback =
      base::Callback<void(mojom::ClientProcess*, bool, OSMemDumpMap)>;

  struct ClientInfo {
    ClientInfo(mojom::ClientProcess* client,
               base::ProcessId pid,
               mojom::ProcessType process_type);
    ~ClientInfo();

    mojom::ClientProcess* client;
    const base::ProcessId pid;
    const mojom::ProcessType process_type;
  };

  // Sets up the parameters of the queued |request| using |clients| and then
  // dispatches the request for a memory dump to each client process.
  static void SetUpAndDispatch(QueuedRequest* request,
                               const std::vector<ClientInfo>& clients,
                               const ChromeCallback& chrome_callback,
                               const OsCallback& os_callback);

  // Finalizes the queued |request| by collating all responses recieved and
  // dispatching to the appropriate callback. Also adds to tracing using
  // |tracing_observer| if the |request| requires it.
  static void Finalize(QueuedRequest* request,
                       TracingObserver* tracing_observer);
};

}  // namespace memory_instrumentation

#endif  // SERVICES_RESOURCE_COORDINATOR_MEMORY_INSTRUMENTATION_QUEUED_REQUEST_DISPATCHER_H_