// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_MEMORY_INSTRUMENTATION_QUEUED_REQUEST_H_
#define SERVICES_RESOURCE_COORDINATOR_MEMORY_INSTRUMENTATION_QUEUED_REQUEST_H_

#include <map>
#include <memory>
#include <set>
#include <unordered_map>

#include "base/trace_event/memory_dump_request_args.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/coordinator.h"
#include "services/resource_coordinator/public/interfaces/memory_instrumentation/memory_instrumentation.mojom.h"

namespace memory_instrumentation {

// Holds data for pending requests enqueued via RequestGlobalMemoryDump().
struct QueuedRequest {
  using OSMemDumpMap =
      std::unordered_map<base::ProcessId,
                         memory_instrumentation::mojom::RawOSMemDumpPtr>;
  using RequestGlobalMemoryDumpInternalCallback = base::Callback<
      void(bool, uint64_t, memory_instrumentation::mojom::GlobalMemoryDumpPtr)>;

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
    std::unique_ptr<base::trace_event::ProcessMemoryDump> chrome_dump;
    OSMemDumpMap os_dumps;
  };

  QueuedRequest(const base::trace_event::MemoryDumpRequestArgs& args,
                const RequestGlobalMemoryDumpInternalCallback& callback,
                bool add_to_trace);
  ~QueuedRequest();

  bool wants_mmaps() const {
    return args.level_of_detail ==
           base::trace_event::MemoryDumpLevelOfDetail::DETAILED;
  }

  // We always want to return chrome dumps, with exception of the special
  // case below for the heap profiler, which cares only about mmaps.
  bool wants_chrome_dumps() const {
    return args.dump_type != base::trace_event::MemoryDumpType::VM_REGIONS_ONLY;
  }

  bool should_return_summaries() const {
    return args.dump_type == base::trace_event::MemoryDumpType::SUMMARY_ONLY;
  }

  const base::trace_event::MemoryDumpRequestArgs args;
  const RequestGlobalMemoryDumpInternalCallback callback;
  const bool add_to_trace;

  // When a dump, requested via RequestGlobalMemoryDump(), is in progress this
  // set contains a |PendingResponse| for each |RequestChromeMemoryDump| and
  // |RequestOSMemoryDump| call that has not yet replied or been canceled (due
  // to the client disconnecting).
  std::set<QueuedRequest::PendingResponse> pending_responses;
  std::map<mojom::ClientProcess*, Response> responses;
  int failed_memory_dump_count = 0;
  bool dump_in_progress = false;

  // The time we started handling the request (does not including queuing
  // time).
  base::Time start_time;
};

}  // namespace memory_instrumentation

#endif  // SERVICES_RESOURCE_COORDINATOR_MEMORY_INSTRUMENTATION_QUEUED_REQUEST_H_