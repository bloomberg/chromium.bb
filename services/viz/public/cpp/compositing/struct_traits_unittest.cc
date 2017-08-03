// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/message_loop/message_loop.h"
#include "components/viz/common/resources/resource_settings.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/viz/public/cpp/compositing/resource_settings_struct_traits.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {

namespace {

using StructTraitsTest = testing::Test;

TEST_F(StructTraitsTest, ResourceSettings) {
  constexpr size_t kArbitrarySize = 32;
  constexpr bool kArbitraryBool = true;
  ResourceSettings input;
  input.texture_id_allocation_chunk_size = kArbitrarySize;
  input.use_gpu_memory_buffer_resources = kArbitraryBool;
  input.buffer_to_texture_target_map =
      DefaultBufferToTextureTargetMapForTesting();

  ResourceSettings output;
  mojom::ResourceSettings::Deserialize(
      mojom::ResourceSettings::Serialize(&input), &output);
  EXPECT_EQ(input.texture_id_allocation_chunk_size,
            output.texture_id_allocation_chunk_size);
  EXPECT_EQ(input.use_gpu_memory_buffer_resources,
            output.use_gpu_memory_buffer_resources);
  EXPECT_EQ(input.buffer_to_texture_target_map,
            output.buffer_to_texture_target_map);
}

}  // namespace

}  // namespace viz
