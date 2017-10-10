// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_MEMORY_INSTRUMENTATION_GRAPH_PROCESSOR_H_
#define SERVICES_RESOURCE_COORDINATOR_MEMORY_INSTRUMENTATION_GRAPH_PROCESSOR_H_

#include <memory>

#include "base/process/process_handle.h"
#include "base/trace_event/process_memory_dump.h"
#include "services/resource_coordinator/memory_instrumentation/graph.h"

namespace memory_instrumentation {

class GraphProcessor {
 public:
  // Processes memory dumps to compute the memory dump graph which allows
  // subsequent computation of metrics such as effective sizes.
  static std::unique_ptr<GlobalDumpGraph> ComputeMemoryGraph(
      const std::map<base::ProcessId, base::trace_event::ProcessMemoryDump>&
          process_dumps);

 private:
  static void CollectAllocatorDumps(
      const base::trace_event::ProcessMemoryDump& source,
      GlobalDumpGraph* global_graph,
      GlobalDumpGraph::Process* process_graph);

  static void AddEdges(const base::trace_event::ProcessMemoryDump& source,
                       GlobalDumpGraph* global_graph);
};

}  // namespace memory_instrumentation
#endif