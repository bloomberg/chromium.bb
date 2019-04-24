// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_COMPOSITOR_FRAME_FUZZER_COMPOSITOR_FRAME_FUZZER_UTIL_H_
#define COMPONENTS_VIZ_SERVICE_COMPOSITOR_FRAME_FUZZER_COMPOSITOR_FRAME_FUZZER_UTIL_H_

#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/service/compositor_frame_fuzzer/compositor_frame_fuzzer.pb.h"

namespace viz {

// Converts a fuzzed specification in the form of a RenderPass protobuf message
// (as defined in compositor_frame_fuzzer.proto) into a CompositorFrame with a
// RenderPass member.
//
// Performs minimal validation and corrections to ensure that submitting the
// frame to a CompositorFrameSink will not result in a mojo deserialization
// validation error.
CompositorFrame GenerateFuzzedCompositorFrame(
    const content::fuzzing::proto::RenderPass& render_pass_spec);

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_COMPOSITOR_FRAME_FUZZER_COMPOSITOR_FRAME_FUZZER_UTIL_H_
