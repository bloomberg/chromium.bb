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

using base::trace_event::MemoryDumpLevelOfDetail;
using base::trace_event::MemoryDumpType;

namespace memory_instrumentation {

// Holds data for pending requests enqueued via RequestGlobalMemoryDump().
struct QueuedRequest {
  using OSMemDumpMap =
      std::unordered_map<base::ProcessId,
                         memory_instrumentation::mojom::RawOSMemDumpPtr>;
  using RequestGlobalMemoryDumpInternalCallback = base::Callback<
      void(bool, uint64_t, memory_instrumentation::mojom::GlobalMemoryDumpPtr)>;

  struct Args {
    Args(MemoryDumpType dump_type,
         MemoryDumpLevelOfDetail level_of_detail,
         const std::vector<std::string>& allocator_dump_names,
         bool add_to_trace,
         base::ProcessId pid);
    Args(const Args&);
    ~Args();

    const MemoryDumpType dump_type;
    const MemoryDumpLevelOfDetail level_of_detail;
    const std::vector<std::string> allocator_dump_names;
    const bool add_to_trace;
    const base::ProcessId pid;
  };

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

  QueuedRequest(const Args& args,
                uint64_t dump_guid,
                const RequestGlobalMemoryDumpInternalCallback& callback);
  ~QueuedRequest();

  base::trace_event::MemoryDumpRequestArgs GetRequestArgs();

  bool wants_mmaps() const {
    return args.level_of_detail == base::trace_event::MemoryDumpLevelOfDetail::
                                       VM_REGIONS_ONLY_FOR_HEAP_PROFILER ||
           args.level_of_detail ==
               base::trace_event::MemoryDumpLevelOfDetail::DETAILED;
  }

  // We always want to return chrome dumps, with exception of the special
  // case below for the heap profiler, which cares only about mmaps.
  bool wants_chrome_dumps() const {
    return args.level_of_detail != base::trace_event::MemoryDumpLevelOfDetail::
                                       VM_REGIONS_ONLY_FOR_HEAP_PROFILER;
  }

  bool should_return_summaries() const {
    return args.dump_type == base::trace_event::MemoryDumpType::SUMMARY_ONLY;
  }

  const Args args;
  const uint64_t dump_guid;
  const RequestGlobalMemoryDumpInternalCallback callback;

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