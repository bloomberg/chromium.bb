// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/memory_instrumentation/graph_processor.h"

#include "base/logging.h"
#include "base/strings/string_split.h"

namespace memory_instrumentation {

using base::CompareCase;
using base::ProcessId;
using base::trace_event::MemoryAllocatorDump;
using base::trace_event::MemoryAllocatorDumpGuid;
using base::trace_event::ProcessMemoryDump;
using Node = memory_instrumentation::GlobalDumpGraph::Node;
using Process = memory_instrumentation::GlobalDumpGraph::Process;

namespace {

Node::Entry::ScalarUnits EntryUnitsFromString(std::string units) {
  if (units == MemoryAllocatorDump::kUnitsBytes) {
    return Node::Entry::ScalarUnits::kBytes;
  } else if (units == MemoryAllocatorDump::kUnitsObjects) {
    return Node::Entry::ScalarUnits::kObjects;
  } else {
    // Invalid units so we just return a value of the correct type.
    return Node::Entry::ScalarUnits::kObjects;
  }
}

void AddEntryToNode(Node* node, const MemoryAllocatorDump::Entry& entry) {
  switch (entry.entry_type) {
    case MemoryAllocatorDump::Entry::EntryType::kUint64:
      node->AddEntry(entry.name, EntryUnitsFromString(entry.units),
                     entry.value_uint64);
      break;
    case MemoryAllocatorDump::Entry::EntryType::kString:
      node->AddEntry(entry.name, entry.value_string);
      break;
  }
}

void CollectAllocatorDumps(const base::trace_event::ProcessMemoryDump& source,
                           GlobalDumpGraph* global_graph,
                           Process* process_graph) {
  for (const auto& path_to_dump : source.allocator_dumps()) {
    const std::string& path = path_to_dump.first;
    const MemoryAllocatorDump& dump = *path_to_dump.second;

    bool is_global = base::StartsWith(path, "global/", CompareCase::SENSITIVE);
    Process* graph =
        is_global ? global_graph->shared_memory_graph() : process_graph;

    Node* node;
    auto node_iterator = global_graph->nodes_by_guid().find(dump.guid());
    if (node_iterator == global_graph->nodes_by_guid().end()) {
      node = graph->CreateNode(dump.guid(), path);
    } else {
      node = node_iterator->second;

      DCHECK_EQ(node, graph->FindNode(path))
          << "Nodes have different paths but same GUIDs";
      DCHECK(is_global) << "Multiple nodes have same GUID without being global";
    }

    // Copy any entries not already present into the node.
    for (auto& entry : dump.entries()) {
      // Check that there are not multiple entries with the same name.
      DCHECK(node->entries().find(entry.name) == node->entries().end());
      AddEntryToNode(node, entry);
    }
  }
}

}  // namespace

std::unique_ptr<GlobalDumpGraph> ComputeMemoryGraph(
    const std::map<ProcessId, ProcessMemoryDump>& process_dumps) {
  auto global_graph = std::make_unique<GlobalDumpGraph>();

  for (auto& pid_to_dump : process_dumps) {
    auto* graph = global_graph->CreateGraphForProcess(pid_to_dump.first);

    // Collects the allocator dumps into a graph and populates the graph
    // with entries.
    CollectAllocatorDumps(pid_to_dump.second, global_graph.get(), graph);
  }
  return global_graph;
}

}  // namespace memory_instrumentation