// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/hit_test/aggregated_hit_test_region.h"
#include "services/viz/public/cpp/hit_test/aggregated_hit_test_region_struct_traits.h"
#include "services/viz/public/interfaces/hit_test/aggregated_hit_test_region.mojom.h"
#include "services/viz/public/interfaces/hit_test/hit_test_region_list.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/mojo/geometry_struct_traits.h"
#include "ui/gfx/mojo/transform_struct_traits.h"

namespace viz {

namespace {

using StructTraitsTest = testing::Test;

// Test StructTrait serialization and deserialization for copyable type. |input|
// will be serialized and then deserialized into |output|.
template <class MojomType, class Type>
void SerializeAndDeserialize(const Type& input, Type* output) {
  MojomType::DeserializeFromMessage(
      mojo::Message(MojomType::SerializeAsMessage(&input).TakeMojoMessage()),
      output);
}

}  // namespace

TEST_F(StructTraitsTest, AggregatedHitTestRegion) {
  constexpr FrameSinkId frame_sink_id(1337, 1234);
  constexpr uint32_t flags = mojom::kHitTestAsk;
  constexpr gfx::Rect rect(1024, 768);
  gfx::Transform transform;
  transform.Scale(.5f, .7f);
  constexpr int32_t child_count = 5;
  AggregatedHitTestRegion input(frame_sink_id, flags, rect, transform,
                                child_count);
  AggregatedHitTestRegion output;
  SerializeAndDeserialize<mojom::AggregatedHitTestRegion>(input, &output);
  EXPECT_EQ(input.frame_sink_id, output.frame_sink_id);
  EXPECT_EQ(input.flags, output.flags);
  EXPECT_EQ(input.rect, output.rect);
  EXPECT_EQ(input.transform(), output.transform());
  EXPECT_EQ(input.child_count, output.child_count);
}

}  // namespace viz
