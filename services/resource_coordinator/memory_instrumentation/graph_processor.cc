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
using Edge = memory_instrumentation::GlobalDumpGraph::Edge;
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

}  // namespace

// static
std::unique_ptr<GlobalDumpGraph> GraphProcessor::ComputeMemoryGraph(
    const std::map<ProcessId, ProcessMemoryDump>& process_dumps) {
  auto global_graph = std::make_unique<GlobalDumpGraph>();

  // First pass: collects allocator dumps into a graph and populate
  // with entries.
  for (const auto& pid_to_dump : process_dumps) {
    auto* graph = global_graph->CreateGraphForProcess(pid_to_dump.first);
    CollectAllocatorDumps(pid_to_dump.second, global_graph.get(), graph);
  }

  // Second pass: generate the graph of edges between the nodes.
  for (const auto& pid_to_dump : process_dumps) {
    AddEdges(pid_to_dump.second, global_graph.get());
  }

  auto* global_root = global_graph->shared_memory_graph()->root();

  // Third pass: mark recursively nodes as weak if they don't have an associated
  // dump and all their children are weak.
  MarkImplicitWeakParentsRecursively(global_root);
  for (const auto& pid_to_process : global_graph->process_dump_graphs()) {
    MarkImplicitWeakParentsRecursively(pid_to_process.second->root());
  }

  // Fourth pass: recursively mark nodes as weak if they own a node which is
  // weak or if they have a parent who is weak.
  {
    std::set<const Node*> visited;
    MarkWeakOwnersAndChildrenRecursively(global_root, &visited);
    for (const auto& pid_to_process : global_graph->process_dump_graphs()) {
      MarkWeakOwnersAndChildrenRecursively(pid_to_process.second->root(),
                                           &visited);
    }
  }

  // Fifth pass: remove all nodes which are weak (including their descendants)
  // and clean owned by edges to match.
  RemoveWeakNodesRecursively(global_root);
  for (const auto& pid_to_process : global_graph->process_dump_graphs()) {
    RemoveWeakNodesRecursively(pid_to_process.second->root());
  }

  return global_graph;
}

// static
void GraphProcessor::CollectAllocatorDumps(
    const base::trace_event::ProcessMemoryDump& source,
    GlobalDumpGraph* global_graph,
    Process* process_graph) {
  // Turn each dump into a node in the graph of dumps in the appropriate
  // process dump or global dump.
  for (const auto& path_to_dump : source.allocator_dumps()) {
    const std::string& path = path_to_dump.first;
    const MemoryAllocatorDump& dump = *path_to_dump.second;

    // All global dumps (i.e. those starting with global/) should be redirected
    // to the shared graph.
    bool is_global = base::StartsWith(path, "global/", CompareCase::SENSITIVE);
    Process* process =
        is_global ? global_graph->shared_memory_graph() : process_graph;

    Node* node;
    auto node_iterator = global_graph->nodes_by_guid().find(dump.guid());
    if (node_iterator == global_graph->nodes_by_guid().end()) {
      // Storing whether the process is weak here will allow for later
      // computations on whether or not the node should be removed.
      bool is_weak = dump.flags() & MemoryAllocatorDump::Flags::WEAK;
      node = process->CreateNode(dump.guid(), path, is_weak);
    } else {
      node = node_iterator->second;

      DCHECK_EQ(node, process->FindNode(path))
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

// static
void GraphProcessor::AddEdges(
    const base::trace_event::ProcessMemoryDump& source,
    GlobalDumpGraph* global_graph) {
  const auto& nodes_by_guid = global_graph->nodes_by_guid();
  for (const auto& guid_to_edge : source.allocator_dumps_edges()) {
    auto& edge = guid_to_edge.second;

    // Find the source and target nodes in the global map by guid.
    auto source_it = nodes_by_guid.find(edge.source);
    auto target_it = nodes_by_guid.find(edge.target);

    if (source_it == nodes_by_guid.end()) {
      // If the source is missing then simply pretend the edge never existed
      // leading to the memory being allocated to the target (if it exists).
      continue;
    } else if (target_it == nodes_by_guid.end()) {
      // If the target is lost but the source is present, then also ignore
      // this edge for now.
      // TODO(lalitm): see crbug.com/770712 for the permanent fix for this
      // issue.
      continue;
    } else {
      // Add an edge indicating the source node owns the memory of the
      // target node with the given importance of the edge.
      global_graph->AddNodeOwnershipEdge(source_it->second, target_it->second,
                                         edge.importance);
    }
  }
}

// static
void GraphProcessor::MarkImplicitWeakParentsRecursively(Node* node) {
  // Ensure that we aren't in a bad state where we have an implicit node
  // which doesn't have any children.
  DCHECK(node->is_explicit() || !node->children()->empty());

  // Check that at this stage, any node which is weak is only so because
  // it was explicitly created as such.
  DCHECK(!node->is_weak() || node->is_explicit());

  // If a node is already weak then all children will be marked weak at a
  // later stage.
  if (node->is_weak())
    return;

  // Recurse into each child and find out if all the children of this node are
  // weak.
  bool all_children_weak = true;
  for (auto& path_to_child : *node->children()) {
    MarkImplicitWeakParentsRecursively(path_to_child.second);
    all_children_weak = all_children_weak && path_to_child.second->is_weak();
  }

  // If all the children are weak and the parent is only an implicit one then we
  // consider the parent as weak as well and we will later remove it.
  node->set_weak(!node->is_explicit() && all_children_weak);
}

// static
void GraphProcessor::MarkWeakOwnersAndChildrenRecursively(
    Node* node,
    std::set<const Node*>* visited) {
  // If we've already visited this node then nothing to do.
  if (visited->count(node) != 0)
    return;

  // If we haven't visited the node which this node owns then wait for that.
  if (node->owns_edge() && visited->count(node->owns_edge()->target()) == 0)
    return;

  // If we haven't visited the node's parent then wait for that.
  if (node->parent() && visited->count(node->parent()) == 0)
    return;

  // If either the node we own or our parent is weak, then mark this node
  // as weak.
  if ((node->owns_edge() && node->owns_edge()->target()->is_weak()) ||
      (node->parent() && node->parent()->is_weak())) {
    node->set_weak(true);
  }
  visited->insert(node);

  // Recurse into each owner node to mark any other nodes.
  for (auto* owned_by_edge : *node->owned_by_edges()) {
    MarkWeakOwnersAndChildrenRecursively(owned_by_edge->source(), visited);
  }

  // Recurse into each child and find out if all the children of this node are
  // weak.
  for (const auto& path_to_child : *node->children()) {
    MarkWeakOwnersAndChildrenRecursively(path_to_child.second, visited);
  }
}

// static
void GraphProcessor::RemoveWeakNodesRecursively(Node* node) {
  auto* children = node->children();
  for (auto child_it = children->begin(); child_it != children->end();) {
    Node* child = child_it->second;

    // If the node is weak, remove it. This automatically makes all
    // descendents unreachable from the parents. If this node owned
    // by another, it will have been marked earlier in
    // |MarkWeakOwnersAndChildrenRecursively| and so will be removed
    // by this method at some point.
    if (child->is_weak()) {
      child_it = children->erase(child_it);
      continue;
    }

    // We should never be in a situation where we're about to
    // keep a node which owns a weak node (which will be/has been
    // removed).
    DCHECK(!child->owns_edge() || !child->owns_edge()->target()->is_weak());

    // Descend and remove all weak child nodes.
    RemoveWeakNodesRecursively(child);

    // Remove all edges with owner nodes which are weak.
    std::vector<Edge*>* owned_by_edges = child->owned_by_edges();
    auto new_end =
        std::remove_if(owned_by_edges->begin(), owned_by_edges->end(),
                       [](Edge* edge) { return edge->source()->is_weak(); });
    owned_by_edges->erase(new_end, owned_by_edges->end());

    ++child_it;
  }
}

}  // namespace memory_instrumentation