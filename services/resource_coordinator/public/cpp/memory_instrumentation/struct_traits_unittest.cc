// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/trace_event/heap_profiler_serialization_state.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation_struct_traits.h"
#include "services/resource_coordinator/public/interfaces/memory_instrumentation/memory_instrumentation.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace memory_instrumentation {

using base::trace_event::MemoryDumpRequestArgs;
using base::trace_event::MemoryDumpLevelOfDetail;
using base::trace_event::MemoryDumpType;

namespace {

using StructTraitsTest = testing::Test;

// Test StructTrait serialization and deserialization for copyable type. |input|
// will be serialized and then deserialized into |output|.
template <class MojomType, class Type>
void SerializeAndDeserialize(const Type& input, Type* output) {
  MojomType::Deserialize(MojomType::Serialize(&input), output);
}

}  // namespace

TEST_F(StructTraitsTest, MemoryDumpRequestArgs) {
  MemoryDumpRequestArgs input{10u, MemoryDumpType::SUMMARY_ONLY,
                              MemoryDumpLevelOfDetail::DETAILED};
  MemoryDumpRequestArgs output;
  SerializeAndDeserialize<mojom::RequestArgs>(input, &output);
  EXPECT_EQ(output.dump_guid, 10u);
  EXPECT_EQ(output.dump_type, MemoryDumpType::SUMMARY_ONLY);
  EXPECT_EQ(output.level_of_detail, MemoryDumpLevelOfDetail::DETAILED);
}

}  // namespace memory_instrumentation
