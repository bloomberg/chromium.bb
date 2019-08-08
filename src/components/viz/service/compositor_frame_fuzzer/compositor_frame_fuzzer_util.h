// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_COMPOSITOR_FRAME_FUZZER_COMPOSITOR_FRAME_FUZZER_UTIL_H_
#define COMPONENTS_VIZ_SERVICE_COMPOSITOR_FRAME_FUZZER_COMPOSITOR_FRAME_FUZZER_UTIL_H_

#include <memory>
#include <vector>

#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/resources/bitmap_allocation.h"
#include "components/viz/service/compositor_frame_fuzzer/compositor_frame_fuzzer.pb.h"

namespace viz {

struct FuzzedBitmap {
  FuzzedBitmap(SharedBitmapId id,
               gfx::Size size,
               std::unique_ptr<base::SharedMemory> shared_memory);
  ~FuzzedBitmap();

  FuzzedBitmap(FuzzedBitmap&& other) noexcept;
  FuzzedBitmap& operator=(FuzzedBitmap&& other) = default;

  SharedBitmapId id;
  gfx::Size size;
  std::unique_ptr<base::SharedMemory> shared_memory;

  DISALLOW_COPY(FuzzedBitmap);
};

struct FuzzedData {
  FuzzedData();
  ~FuzzedData();

  FuzzedData(FuzzedData&& other) noexcept;
  FuzzedData& operator=(FuzzedData&& other) = default;

  CompositorFrame frame;
  std::vector<FuzzedBitmap> allocated_bitmaps;

  DISALLOW_COPY(FuzzedData);
};

// Converts a fuzzed specification in the form of a RenderPass protobuf message
// (as defined in compositor_frame_fuzzer.proto) into a CompositorFrame with a
// RenderPass member.
//
// Performs minimal validation and corrections to ensure that submitting the
// frame to a CompositorFrameSink will not result in a mojo deserialization
// validation error.
FuzzedData GenerateFuzzedCompositorFrame(
    const content::fuzzing::proto::RenderPass& render_pass_spec);

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_COMPOSITOR_FRAME_FUZZER_COMPOSITOR_FRAME_FUZZER_UTIL_H_
