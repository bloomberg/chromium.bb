// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/memory_instrumentation/graph.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace memory_instrumentation {

using base::trace_event::MemoryAllocatorDumpGuid;
using Node = memory_instrumentation::GlobalDumpGraph::Node;
using Process = memory_instrumentation::GlobalDumpGraph::Process;

TEST(GlobalDumpGraphTest, CreateContainerForProcess) {
  GlobalDumpGraph global_dump_graph;

  Process* dump = global_dump_graph.CreateGraphForProcess(10);
  ASSERT_NE(dump, nullptr);

  auto* map = global_dump_graph.process_dump_graphs().find(10)->second.get();
  ASSERT_EQ(dump, map);
}

TEST(GlobalDumpGraphTest, AddNodeOwnershipEdge) {
  GlobalDumpGraph global_dump_graph;
  Node owner(global_dump_graph.shared_memory_graph(), nullptr);
  Node owned(global_dump_graph.shared_memory_graph(), nullptr);

  global_dump_graph.AddNodeOwnershipEdge(&owner, &owned, 1);

  auto& edges = global_dump_graph.edges();
  ASSERT_NE(edges.begin(), edges.end());

  auto& edge = *edges.begin();
  ASSERT_EQ(edge.source(), &owner);
  ASSERT_EQ(edge.target(), &owned);
  ASSERT_EQ(edge.priority(), 1);
}

TEST(ProcessTest, CreateAndFindNode) {
  GlobalDumpGraph global_dump_graph;
  Process graph(&global_dump_graph);

  Node* first = graph.CreateNode(MemoryAllocatorDumpGuid(1), "simple/test/1");
  Node* second = graph.CreateNode(MemoryAllocatorDumpGuid(2), "simple/test/2");
  Node* third = graph.CreateNode(MemoryAllocatorDumpGuid(3), "simple/other/1");
  Node* fourth = graph.CreateNode(MemoryAllocatorDumpGuid(4), "complex/path");
  Node* fifth =
      graph.CreateNode(MemoryAllocatorDumpGuid(5), "complex/path/child/1");

  ASSERT_EQ(graph.FindNode("simple/test/1"), first);
  ASSERT_EQ(graph.FindNode("simple/test/2"), second);
  ASSERT_EQ(graph.FindNode("simple/other/1"), third);
  ASSERT_EQ(graph.FindNode("complex/path"), fourth);
  ASSERT_EQ(graph.FindNode("complex/path/child/1"), fifth);

  auto& nodes_by_guid = global_dump_graph.nodes_by_guid();
  ASSERT_EQ(nodes_by_guid.find(MemoryAllocatorDumpGuid(1))->second, first);
  ASSERT_EQ(nodes_by_guid.find(MemoryAllocatorDumpGuid(2))->second, second);
  ASSERT_EQ(nodes_by_guid.find(MemoryAllocatorDumpGuid(3))->second, third);
  ASSERT_EQ(nodes_by_guid.find(MemoryAllocatorDumpGuid(4))->second, fourth);
  ASSERT_EQ(nodes_by_guid.find(MemoryAllocatorDumpGuid(5))->second, fifth);
}

TEST(ProcessTest, CreateNodeParent) {
  GlobalDumpGraph global_dump_graph;
  Process graph(&global_dump_graph);

  Node* parent = graph.CreateNode(MemoryAllocatorDumpGuid(1), "simple");
  Node* child = graph.CreateNode(MemoryAllocatorDumpGuid(1), "simple/child");

  ASSERT_EQ(parent->parent(), graph.root());
  ASSERT_EQ(child->parent(), parent);
}

TEST(NodeTest, GetChild) {
  GlobalDumpGraph global_dump_graph;
  Node node(global_dump_graph.shared_memory_graph(), nullptr);

  ASSERT_EQ(node.GetChild("test"), nullptr);

  Node child(global_dump_graph.shared_memory_graph(), &node);
  node.InsertChild("child", &child);
  ASSERT_EQ(node.GetChild("child"), &child);
}

TEST(NodeTest, InsertChild) {
  GlobalDumpGraph global_dump_graph;
  Node node(global_dump_graph.shared_memory_graph(), nullptr);

  ASSERT_EQ(node.GetChild("test"), nullptr);

  Node child(global_dump_graph.shared_memory_graph(), &node);
  node.InsertChild("child", &child);
  ASSERT_EQ(node.GetChild("child"), &child);
}

TEST(NodeTest, AddEntry) {
  GlobalDumpGraph global_dump_graph;
  Node node(global_dump_graph.shared_memory_graph(), nullptr);

  node.AddEntry("scalar", Node::Entry::ScalarUnits::kBytes, 100ul);
  ASSERT_EQ(node.entries().size(), 1ul);

  node.AddEntry("string", "data");
  ASSERT_EQ(node.entries().size(), 2ul);

  auto scalar = node.entries().find("scalar");
  ASSERT_EQ(scalar->first, "scalar");
  ASSERT_EQ(scalar->second.units, Node::Entry::ScalarUnits::kBytes);
  ASSERT_EQ(scalar->second.value_uint64, 100ul);

  auto string = node.entries().find("string");
  ASSERT_EQ(string->first, "string");
  ASSERT_EQ(string->second.value_string, "data");
}

}  // namespace memory_instrumentation