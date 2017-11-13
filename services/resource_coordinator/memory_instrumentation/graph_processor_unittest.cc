// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/memory_instrumentation/graph_processor.h"

#include "base/memory/shared_memory_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace memory_instrumentation {

using base::ProcessId;
using base::trace_event::HeapProfilerSerializationState;
using base::trace_event::MemoryAllocatorDump;
using base::trace_event::MemoryAllocatorDumpGuid;
using base::trace_event::MemoryDumpArgs;
using base::trace_event::MemoryDumpLevelOfDetail;
using base::trace_event::ProcessMemoryDump;
using Edge = GlobalDumpGraph::Edge;
using Node = GlobalDumpGraph::Node;
using Process = GlobalDumpGraph::Process;

class GraphProcessorTest : public testing::Test {
 public:
  GraphProcessorTest() {}

  void MarkImplicitWeakParentsRecursively(Node* node) {
    GraphProcessor::MarkImplicitWeakParentsRecursively(node);
  }

  void MarkWeakOwnersAndChildrenRecursively(Node* node) {
    std::set<const Node*> visited;
    GraphProcessor::MarkWeakOwnersAndChildrenRecursively(node, &visited);
  }

  void RemoveWeakNodesRecursively(Node* node) {
    GraphProcessor::RemoveWeakNodesRecursively(node);
  }

  void AssignTracingOverhead(base::StringPiece allocator,
                             GlobalDumpGraph* global_graph,
                             GlobalDumpGraph::Process* process) {
    GraphProcessor::AssignTracingOverhead(allocator, global_graph, process);
  }

  GlobalDumpGraph::Node::Entry AggregateNumericWithNameForNode(
      GlobalDumpGraph::Node* node,
      base::StringPiece name) {
    return GraphProcessor::AggregateNumericWithNameForNode(node, name);
  }

  void AggregateNumericsRecursively(GlobalDumpGraph::Node* node) {
    return GraphProcessor::AggregateNumericsRecursively(node);
  }

  void PropagateNumericsAndDiagnosticsRecursively(GlobalDumpGraph::Node* node) {
    return GraphProcessor::PropagateNumericsAndDiagnosticsRecursively(node);
  }
};

TEST_F(GraphProcessorTest, SmokeComputeMemoryGraph) {
  std::map<ProcessId, const ProcessMemoryDump*> process_dumps;

  MemoryDumpArgs dump_args = {MemoryDumpLevelOfDetail::DETAILED};
  ProcessMemoryDump pmd(new HeapProfilerSerializationState, dump_args);

  auto* source = pmd.CreateAllocatorDump("test1/test2/test3");
  source->AddScalar(MemoryAllocatorDump::kNameSize,
                    MemoryAllocatorDump::kUnitsBytes, 10);

  auto* target = pmd.CreateAllocatorDump("target");
  pmd.AddOwnershipEdge(source->guid(), target->guid(), 10);

  auto* weak =
      pmd.CreateWeakSharedGlobalAllocatorDump(MemoryAllocatorDumpGuid(1));

  process_dumps.emplace(1, &pmd);

  auto global_dump = GraphProcessor::CreateMemoryGraph(process_dumps);

  ASSERT_EQ(1u, global_dump->process_dump_graphs().size());

  auto id_to_dump_it = global_dump->process_dump_graphs().find(1);
  auto* first_child = id_to_dump_it->second->FindNode("test1");
  ASSERT_NE(first_child, nullptr);
  ASSERT_EQ(first_child->parent(), id_to_dump_it->second->root());

  auto* second_child = first_child->GetChild("test2");
  ASSERT_NE(second_child, nullptr);
  ASSERT_EQ(second_child->parent(), first_child);

  auto* third_child = second_child->GetChild("test3");
  ASSERT_NE(third_child, nullptr);
  ASSERT_EQ(third_child->parent(), second_child);

  auto* direct = id_to_dump_it->second->FindNode("test1/test2/test3");
  ASSERT_EQ(third_child, direct);

  ASSERT_EQ(third_child->entries()->size(), 1ul);

  auto size = third_child->entries()->find(MemoryAllocatorDump::kNameSize);
  ASSERT_EQ(10ul, size->second.value_uint64);

  ASSERT_TRUE(weak->flags() & MemoryAllocatorDump::Flags::WEAK);

  auto& edges = global_dump->edges();
  auto edge_it = edges.begin();
  ASSERT_EQ(std::distance(edges.begin(), edges.end()), 1l);
  ASSERT_EQ(edge_it->source(), direct);
  ASSERT_EQ(edge_it->target(), id_to_dump_it->second->FindNode("target"));
  ASSERT_EQ(edge_it->priority(), 10);
}

TEST_F(GraphProcessorTest, SmokeComputeSharedFootprint) {
  std::map<ProcessId, const ProcessMemoryDump*> process_dumps;

  MemoryDumpArgs dump_args = {MemoryDumpLevelOfDetail::DETAILED};
  ProcessMemoryDump pmd1(new HeapProfilerSerializationState, dump_args);
  ProcessMemoryDump pmd2(new HeapProfilerSerializationState, dump_args);

  auto* p1_d1 = pmd1.CreateAllocatorDump("process1/dump1");
  auto* p1_d2 = pmd1.CreateAllocatorDump("process1/dump2");
  auto* p2_d1 = pmd2.CreateAllocatorDump("process2/dump1");

  base::UnguessableToken token = base::UnguessableToken::Create();

  // Done by SharedMemoryTracker.
  size_t size = 256;
  auto global_dump_guid =
      base::SharedMemoryTracker::GetGlobalDumpIdForTracing(token);
  auto local_dump_name =
      base::SharedMemoryTracker::GetDumpNameForTracing(token);

  MemoryAllocatorDump* local_dump_1 =
      pmd1.CreateAllocatorDump(local_dump_name, MemoryAllocatorDumpGuid(1));
  local_dump_1->AddScalar("virtual_size", MemoryAllocatorDump::kUnitsBytes,
                          size);
  local_dump_1->AddScalar(MemoryAllocatorDump::kNameSize,
                          MemoryAllocatorDump::kUnitsBytes, size);

  MemoryAllocatorDump* global_dump_1 =
      pmd1.CreateSharedGlobalAllocatorDump(global_dump_guid);
  global_dump_1->AddScalar(MemoryAllocatorDump::kNameSize,
                           MemoryAllocatorDump::kUnitsBytes, size);

  pmd1.AddOverridableOwnershipEdge(local_dump_1->guid(), global_dump_1->guid(),
                                   0 /* importance */);

  MemoryAllocatorDump* local_dump_2 =
      pmd2.CreateAllocatorDump(local_dump_name, MemoryAllocatorDumpGuid(2));
  local_dump_2->AddScalar("virtual_size", MemoryAllocatorDump::kUnitsBytes,
                          size);
  local_dump_2->AddScalar(MemoryAllocatorDump::kNameSize,
                          MemoryAllocatorDump::kUnitsBytes, size);

  MemoryAllocatorDump* global_dump_2 =
      pmd2.CreateSharedGlobalAllocatorDump(global_dump_guid);
  pmd2.AddOverridableOwnershipEdge(local_dump_2->guid(), global_dump_2->guid(),
                                   0 /* importance */);

  // Done by each consumer of the shared memory.
  pmd1.CreateSharedMemoryOwnershipEdge(p1_d1->guid(), token, 2);
  pmd1.CreateSharedMemoryOwnershipEdge(p1_d2->guid(), token, 1);
  pmd2.CreateSharedMemoryOwnershipEdge(p2_d1->guid(), token, 2);

  process_dumps.emplace(1, &pmd1);
  process_dumps.emplace(2, &pmd2);

  auto graph = GraphProcessor::CreateMemoryGraph(process_dumps);
  auto pid_to_sizes = GraphProcessor::ComputeSharedFootprintFromGraph(*graph);
  ASSERT_EQ(pid_to_sizes[1], 128ul);
  ASSERT_EQ(pid_to_sizes[2], 128ul);
}

TEST_F(GraphProcessorTest, ComputeSharedFootprintFromGraphSameImportance) {
  GlobalDumpGraph graph;
  Process* global_process = graph.shared_memory_graph();
  Node* global_node =
      global_process->CreateNode(MemoryAllocatorDumpGuid(1), "global/1", false);
  global_node->AddEntry("size", Node::Entry::ScalarUnits::kBytes, 100);

  Process* first = graph.CreateGraphForProcess(1);
  Node* shared_1 =
      first->CreateNode(MemoryAllocatorDumpGuid(2), "shared_memory/1", false);

  Process* second = graph.CreateGraphForProcess(2);
  Node* shared_2 =
      second->CreateNode(MemoryAllocatorDumpGuid(3), "shared_memory/2", false);

  graph.AddNodeOwnershipEdge(shared_1, global_node, 1);
  graph.AddNodeOwnershipEdge(shared_2, global_node, 1);

  auto pid_to_sizes = GraphProcessor::ComputeSharedFootprintFromGraph(graph);
  ASSERT_EQ(pid_to_sizes[1], 50ul);
  ASSERT_EQ(pid_to_sizes[2], 50ul);
}

TEST_F(GraphProcessorTest, ComputeSharedFootprintFromGraphSomeDiffImportance) {
  GlobalDumpGraph graph;
  Process* global_process = graph.shared_memory_graph();
  Node* global_node =
      global_process->CreateNode(MemoryAllocatorDumpGuid(1), "global/1", false);
  global_node->AddEntry("size", Node::Entry::ScalarUnits::kBytes, 100);

  Process* first = graph.CreateGraphForProcess(1);
  Node* shared_1 =
      first->CreateNode(MemoryAllocatorDumpGuid(2), "shared_memory/1", false);

  Process* second = graph.CreateGraphForProcess(2);
  Node* shared_2 =
      second->CreateNode(MemoryAllocatorDumpGuid(3), "shared_memory/2", false);

  Process* third = graph.CreateGraphForProcess(3);
  Node* shared_3 =
      third->CreateNode(MemoryAllocatorDumpGuid(4), "shared_memory/3", false);

  Process* fourth = graph.CreateGraphForProcess(4);
  Node* shared_4 =
      fourth->CreateNode(MemoryAllocatorDumpGuid(5), "shared_memory/4", false);

  Process* fifth = graph.CreateGraphForProcess(5);
  Node* shared_5 =
      fifth->CreateNode(MemoryAllocatorDumpGuid(6), "shared_memory/5", false);

  graph.AddNodeOwnershipEdge(shared_1, global_node, 1);
  graph.AddNodeOwnershipEdge(shared_2, global_node, 2);
  graph.AddNodeOwnershipEdge(shared_3, global_node, 3);
  graph.AddNodeOwnershipEdge(shared_4, global_node, 3);
  graph.AddNodeOwnershipEdge(shared_5, global_node, 3);

  auto pid_to_sizes = GraphProcessor::ComputeSharedFootprintFromGraph(graph);
  ASSERT_EQ(pid_to_sizes[1], 0ul);
  ASSERT_EQ(pid_to_sizes[2], 0ul);
  ASSERT_EQ(pid_to_sizes[3], 33ul);
  ASSERT_EQ(pid_to_sizes[4], 33ul);
  ASSERT_EQ(pid_to_sizes[5], 33ul);
}

TEST_F(GraphProcessorTest, MarkWeakParentsSimple) {
  GlobalDumpGraph graph;
  Node parent(graph.shared_memory_graph(), nullptr);
  Node first(graph.shared_memory_graph(), &parent);
  Node second(graph.shared_memory_graph(), &parent);

  parent.InsertChild("first", &first);
  parent.InsertChild("second", &second);

  // Case where one child is not weak.
  parent.set_explicit(false);
  parent.set_weak(false);
  first.set_explicit(true);
  first.set_weak(true);
  second.set_explicit(true);
  second.set_weak(false);

  // The function should be a no-op.
  MarkImplicitWeakParentsRecursively(&parent);
  ASSERT_FALSE(parent.is_weak());
  ASSERT_TRUE(first.is_weak());
  ASSERT_FALSE(second.is_weak());

  // Case where all children is weak.
  second.set_weak(true);

  // The function should mark parent as weak.
  MarkImplicitWeakParentsRecursively(&parent);
  ASSERT_TRUE(parent.is_weak());
  ASSERT_TRUE(first.is_weak());
  ASSERT_TRUE(second.is_weak());
}

TEST_F(GraphProcessorTest, MarkWeakParentsComplex) {
  GlobalDumpGraph graph;
  Node parent(graph.shared_memory_graph(), nullptr);
  Node first(graph.shared_memory_graph(), &parent);
  Node first_child(graph.shared_memory_graph(), &first);
  Node first_grandchild(graph.shared_memory_graph(), &first_child);

  parent.InsertChild("first", &first);
  first.InsertChild("child", &first_child);
  first_child.InsertChild("child", &first_grandchild);

  // |first| is explicitly storng but |first_child| is implicitly so.
  parent.set_explicit(false);
  parent.set_weak(false);
  first.set_explicit(true);
  first.set_weak(false);
  first_child.set_explicit(false);
  first_child.set_weak(false);
  first_grandchild.set_weak(true);
  first_grandchild.set_explicit(true);

  // That should lead to |first_child| marked implicitly weak.
  MarkImplicitWeakParentsRecursively(&parent);
  ASSERT_FALSE(parent.is_weak());
  ASSERT_FALSE(first.is_weak());
  ASSERT_TRUE(first_child.is_weak());
  ASSERT_TRUE(first_grandchild.is_weak());

  // Reset and change so that first is now only implicitly strong.
  first.set_explicit(false);
  first_child.set_weak(false);

  // The whole chain should now be weak.
  MarkImplicitWeakParentsRecursively(&parent);
  ASSERT_TRUE(parent.is_weak());
  ASSERT_TRUE(first.is_weak());
  ASSERT_TRUE(first_child.is_weak());
  ASSERT_TRUE(first_grandchild.is_weak());
}

TEST_F(GraphProcessorTest, MarkWeakOwners) {
  GlobalDumpGraph graph;
  Node owner(graph.shared_memory_graph(), nullptr);
  Node owned(graph.shared_memory_graph(), nullptr);
  Node owned_2(graph.shared_memory_graph(), nullptr);

  Edge edge_1(&owner, &owned, 0);
  Edge edge_2(&owned, &owned_2, 0);

  owner.SetOwnsEdge(&edge_1);
  owned.AddOwnedByEdge(&edge_1);

  owned.SetOwnsEdge(&edge_2);
  owned_2.AddOwnedByEdge(&edge_2);

  // Make only the ultimate owned node weak.
  owner.set_weak(false);
  owned.set_weak(false);
  owned_2.set_weak(true);

  // Starting from leaf node should lead to everything being weak.
  MarkWeakOwnersAndChildrenRecursively(&owned_2);
  ASSERT_TRUE(owner.is_weak());
  ASSERT_TRUE(owned.is_weak());
  ASSERT_TRUE(owned_2.is_weak());
}

TEST_F(GraphProcessorTest, MarkWeakParent) {
  GlobalDumpGraph graph;
  Node parent(graph.shared_memory_graph(), nullptr);
  Node child(graph.shared_memory_graph(), &parent);
  Node child_2(graph.shared_memory_graph(), &child);

  parent.InsertChild("child", &child);
  child.InsertChild("child", &child_2);

  // Make only the ultimate parent node weak.
  parent.set_weak(true);
  child.set_weak(false);
  child_2.set_weak(false);

  // Starting from parent node should lead to everything being weak.
  MarkWeakOwnersAndChildrenRecursively(&parent);
  ASSERT_TRUE(parent.is_weak());
  ASSERT_TRUE(child.is_weak());
  ASSERT_TRUE(child_2.is_weak());
}

TEST_F(GraphProcessorTest, MarkWeakParentOwner) {
  GlobalDumpGraph graph;
  Node parent(graph.shared_memory_graph(), nullptr);
  Node child(graph.shared_memory_graph(), &parent);
  Node child_2(graph.shared_memory_graph(), &child);

  parent.InsertChild("child", &child);
  child.InsertChild("child", &child_2);

  Node owner(graph.shared_memory_graph(), nullptr);

  Edge edge(&owner, &parent, 0);
  owner.SetOwnsEdge(&edge);
  parent.AddOwnedByEdge(&edge);

  // Make only the parent node weak.
  parent.set_weak(true);
  child.set_weak(false);
  child_2.set_weak(false);
  owner.set_weak(false);

  // Starting from parent node should lead to everything being weak.
  MarkWeakOwnersAndChildrenRecursively(&parent);
  ASSERT_TRUE(parent.is_weak());
  ASSERT_TRUE(child.is_weak());
  ASSERT_TRUE(child_2.is_weak());
  ASSERT_TRUE(owner.is_weak());
}

TEST_F(GraphProcessorTest, RemoveWeakNodesRecursively) {
  GlobalDumpGraph graph;
  Node parent(graph.shared_memory_graph(), nullptr);
  Node child(graph.shared_memory_graph(), &parent);
  Node child_2(graph.shared_memory_graph(), &child);
  Node owned(graph.shared_memory_graph(), &parent);

  parent.InsertChild("owned", &owned);
  parent.InsertChild("child", &child);
  child.InsertChild("child", &child_2);

  Edge edge(&child, &owned, 0);
  owned.AddOwnedByEdge(&edge);
  child.SetOwnsEdge(&edge);

  // Make only the parent node weak.
  child.set_weak(true);
  child_2.set_weak(false);
  owned.set_weak(false);

  // Starting from parent node should lead child and child_2 being
  // removed and owned to have the edge from it removed.
  RemoveWeakNodesRecursively(&parent);

  ASSERT_EQ(parent.children()->size(), 1ul);
  ASSERT_EQ(parent.children()->begin()->second, &owned);

  ASSERT_TRUE(owned.owned_by_edges()->empty());
}

TEST_F(GraphProcessorTest, RemoveWeakNodesRecursivelyBetweenGraphs) {
  GlobalDumpGraph graph;
  GlobalDumpGraph::Process first_process(1, &graph);
  GlobalDumpGraph::Process second_process(2, &graph);

  Node parent(&first_process, first_process.root());
  Node child(&first_process, &parent);
  Node child_2(&first_process, &child);
  Node owned(&second_process, second_process.root());

  first_process.root()->InsertChild("parent", &parent);
  parent.InsertChild("child", &child);
  child.InsertChild("child", &child_2);

  second_process.root()->InsertChild("owned", &owned);

  Edge edge(&child, &owned, 0);
  owned.AddOwnedByEdge(&edge);
  child.SetOwnsEdge(&edge);

  // Make only the parent node weak.
  child.set_weak(true);
  child_2.set_weak(false);
  owned.set_weak(false);

  // Starting from parent node should lead child and child_2 being
  // removed.
  RemoveWeakNodesRecursively(first_process.root());

  ASSERT_EQ(first_process.root()->children()->size(), 1ul);
  ASSERT_EQ(parent.children()->size(), 0ul);
  ASSERT_EQ(second_process.root()->children()->size(), 1ul);

  // This should be false until our next pass.
  ASSERT_FALSE(owned.owned_by_edges()->empty());

  RemoveWeakNodesRecursively(second_process.root());

  // We should now have cleaned up the owned node's edges.
  ASSERT_TRUE(owned.owned_by_edges()->empty());
}

TEST_F(GraphProcessorTest, AssignTracingOverhead) {
  GlobalDumpGraph graph;
  Process process(1, &graph);

  // Now add an allocator node.
  Node allocator(&process, process.root());
  process.root()->InsertChild("malloc", &allocator);

  // If the tracing node does not exist, this should do nothing.
  AssignTracingOverhead("malloc", &graph, &process);
  ASSERT_TRUE(process.root()->GetChild("malloc")->children()->empty());

  // Now add a tracing node.
  Node tracing(&process, process.root());
  process.root()->InsertChild("tracing", &tracing);

  // This should now add a node with the allocator.
  AssignTracingOverhead("malloc", &graph, &process);
  ASSERT_NE(process.FindNode("malloc/allocated_objects/tracing_overhead"),
            nullptr);
}

TEST_F(GraphProcessorTest, AggregateNumericWithNameForNode) {
  GlobalDumpGraph graph;
  Process process(1, &graph);

  Node parent(&process, process.root());

  Node c1(&process, &parent);
  c1.AddEntry("random_numeric", Node::Entry::ScalarUnits::kBytes, 100);

  Node c2(&process, &parent);
  c2.AddEntry("random_numeric", Node::Entry::ScalarUnits::kBytes, 256);

  Node c3(&process, &parent);
  c3.AddEntry("other_numeric", Node::Entry::ScalarUnits::kBytes, 1000);

  parent.InsertChild("c1", &c1);
  parent.InsertChild("c2", &c2);
  parent.InsertChild("c3", &c3);

  Node::Entry entry =
      AggregateNumericWithNameForNode(&parent, "random_numeric");
  ASSERT_EQ(entry.value_uint64, 356ul);
  ASSERT_EQ(entry.units, Node::Entry::ScalarUnits::kBytes);
}

TEST_F(GraphProcessorTest, AggregateNumericsRecursively) {
  GlobalDumpGraph graph;
  Process process(1, &graph);
  Node parent(&process, process.root());

  Node c1(&process, &parent);
  c1.AddEntry("random_numeric", Node::Entry::ScalarUnits::kBytes, 100);

  // If an entry already exists in the parent, the child should not
  // ovewrite it.
  Node c2(&process, &parent);
  Node c2_c1(&process, &c2);
  Node c2_c2(&process, &c2);
  c2.AddEntry("random_numeric", Node::Entry::ScalarUnits::kBytes, 256);
  c2_c1.AddEntry("random_numeric", Node::Entry::ScalarUnits::kBytes, 256);
  c2_c2.AddEntry("random_numeric", Node::Entry::ScalarUnits::kBytes, 256);

  // If nothing exists, then the child can aggregrate.
  Node c3(&process, &parent);
  Node c3_c1(&process, &c3);
  Node c3_c2(&process, &c3);
  c3_c1.AddEntry("random_numeric", Node::Entry::ScalarUnits::kBytes, 10);
  c3_c2.AddEntry("random_numeric", Node::Entry::ScalarUnits::kBytes, 10);

  parent.InsertChild("c1", &c1);
  parent.InsertChild("c2", &c2);
  parent.InsertChild("c3", &c3);
  c2.InsertChild("c1", &c2_c1);
  c2.InsertChild("c2", &c2_c2);
  c3.InsertChild("c1", &c3_c1);
  c3.InsertChild("c2", &c3_c2);

  AggregateNumericsRecursively(&parent);
  ASSERT_EQ(parent.entries()->size(), 1ul);

  auto entry = parent.entries()->begin()->second;
  ASSERT_EQ(entry.value_uint64, 376ul);
  ASSERT_EQ(entry.units, Node::Entry::ScalarUnits::kBytes);
}

TEST_F(GraphProcessorTest, PropagateNumericsAndDiagnosticsRecursively) {
  GlobalDumpGraph graph;
  Process process(1, &graph);

  Node c1(&process, process.root());
  Node c1_c1(&process, process.root());

  Node c2(&process, process.root());

  Node owner_1(&process, process.root());
  Node owner_2(&process, process.root());

  process.root()->InsertChild("c1", &c1);
  process.root()->InsertChild("c2", &c2);
  process.root()->InsertChild("owner_1", &owner_1);
  process.root()->InsertChild("owner_2", &owner_2);

  c1.InsertChild("c1", &c1_c1);

  Edge edge_1(&owner_1, &c1_c1, 0);
  c1_c1.AddOwnedByEdge(&edge_1);
  owner_1.SetOwnsEdge(&edge_1);

  Edge edge_2(&owner_2, &c2, 0);
  c2.AddOwnedByEdge(&edge_2);
  owner_2.SetOwnsEdge(&edge_2);
}

}  // namespace memory_instrumentation