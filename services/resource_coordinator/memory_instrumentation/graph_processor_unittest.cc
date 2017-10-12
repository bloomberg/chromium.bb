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

TEST(GraphProcessorTest, ComputeMemoryGraph) {
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

}  // namespace memory_instrumentation