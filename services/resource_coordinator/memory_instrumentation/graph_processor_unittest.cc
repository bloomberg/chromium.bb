// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/memory_instrumentation/graph_processor.h"

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
};

TEST_F(GraphProcessorTest, SmokeComputeMemoryGraph) {
  std::map<ProcessId, ProcessMemoryDump> process_dumps;

  MemoryDumpArgs dump_args = {MemoryDumpLevelOfDetail::DETAILED};
  ProcessMemoryDump pmd(new HeapProfilerSerializationState, dump_args);

  auto* source = pmd.CreateAllocatorDump("test1/test2/test3");
  source->AddScalar(MemoryAllocatorDump::kNameSize,
                    MemoryAllocatorDump::kUnitsBytes, 10);

  auto* target = pmd.CreateAllocatorDump("target");
  pmd.AddOwnershipEdge(source->guid(), target->guid(), 10);

  auto* weak =
      pmd.CreateWeakSharedGlobalAllocatorDump(MemoryAllocatorDumpGuid(1));

  process_dumps.emplace(1, std::move(pmd));

  auto global_dump =
      GraphProcessor::ComputeMemoryGraph(std::move(process_dumps));

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

  ASSERT_EQ(third_child->entries().size(), 1ul);

  auto size = third_child->entries().find(MemoryAllocatorDump::kNameSize);
  ASSERT_EQ(10ul, size->second.value_uint64);

  ASSERT_TRUE(weak->flags() & MemoryAllocatorDump::Flags::WEAK);

  auto& edges = global_dump->edges();
  auto edge_it = edges.begin();
  ASSERT_EQ(std::distance(edges.begin(), edges.end()), 1l);
  ASSERT_EQ(edge_it->source(), direct);
  ASSERT_EQ(edge_it->target(), id_to_dump_it->second->FindNode("target"));
  ASSERT_EQ(edge_it->priority(), 10);
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

}  // namespace memory_instrumentation