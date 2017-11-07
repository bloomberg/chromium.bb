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
  using MemoryDumpMap =
      std::map<base::ProcessId, base::trace_event::ProcessMemoryDump>;

  static std::unique_ptr<GlobalDumpGraph> CreateMemoryGraph(
      const MemoryDumpMap& process_dumps);

  static void RemoveWeakNodesFromGraph(GlobalDumpGraph* global_graph);

  static std::map<base::ProcessId, uint64_t> ComputeSharedFootprintFromGraph(
      const GlobalDumpGraph& global_graph);

 private:
  friend class GraphProcessorTest;

  static void CollectAllocatorDumps(
      const base::trace_event::ProcessMemoryDump& source,
      GlobalDumpGraph* global_graph,
      GlobalDumpGraph::Process* process_graph);

  static void AddEdges(const base::trace_event::ProcessMemoryDump& source,
                       GlobalDumpGraph* global_graph);

  static void MarkImplicitWeakParentsRecursively(GlobalDumpGraph::Node* node);

  static void MarkWeakOwnersAndChildrenRecursively(
      GlobalDumpGraph::Node* node,
      std::set<const GlobalDumpGraph::Node*>* nodes);

  static void RemoveWeakNodesRecursively(GlobalDumpGraph::Node* parent);

  static void AssignTracingOverhead(base::StringPiece allocator,
                                    GlobalDumpGraph* global_graph,
                                    GlobalDumpGraph::Process* process);
};

}  // namespace memory_instrumentation
#endif