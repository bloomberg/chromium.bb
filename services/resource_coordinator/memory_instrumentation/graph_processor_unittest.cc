// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/memory_instrumentation/graph_processor.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace memory_instrumentation {

using base::ProcessId;
using base::trace_event::HeapProfilerSerializationState;
using base::trace_event::MemoryAllocatorDump;
using base::trace_event::MemoryDumpArgs;
using base::trace_event::MemoryDumpLevelOfDetail;
using base::trace_event::ProcessMemoryDump;

TEST(GraphProcessorTest, ComputeMemoryGraph) {
  std::map<ProcessId, ProcessMemoryDump> process_dumps;

  MemoryDumpArgs dump_args = {MemoryDumpLevelOfDetail::DETAILED};
  ProcessMemoryDump pmd(new HeapProfilerSerializationState, dump_args);

  auto* allocator_dump = pmd.CreateAllocatorDump("test1/test2/test3");
  allocator_dump->AddScalar(MemoryAllocatorDump::kNameSize,
                            MemoryAllocatorDump::kUnitsBytes, 10);

  process_dumps.emplace(1, std::move(pmd));

  auto global_dump = ComputeMemoryGraph(std::move(process_dumps));

  ASSERT_EQ(1u, global_dump->process_dump_graphs().size());

  auto id_to_dump_it = global_dump->process_dump_graphs().find(1);
  auto* first_child = id_to_dump_it->second->FindNode("test1");
  ASSERT_TRUE(first_child != nullptr);

  auto* second_child = first_child->GetChild("test2");
  ASSERT_TRUE(second_child != nullptr);

  auto* third_child = second_child->GetChild("test3");
  ASSERT_TRUE(third_child != nullptr);

  auto* direct = id_to_dump_it->second->FindNode("test1/test2/test3");
  ASSERT_TRUE(third_child == direct);

  ASSERT_EQ(third_child->entries().size(), 1ul);

  auto size = third_child->entries().find(MemoryAllocatorDump::kNameSize);
  ASSERT_EQ(10ul, size->second.value_uint64);
}

}  // namespace memory_instrumentation