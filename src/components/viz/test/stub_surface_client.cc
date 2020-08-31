// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/stub_surface_client.h"

#include "components/viz/common/frame_sinks/copy_output_request.h"

namespace viz {

StubSurfaceClient::StubSurfaceClient() = default;

StubSurfaceClient::~StubSurfaceClient() = default;

std::vector<std::unique_ptr<CopyOutputRequest>>
StubSurfaceClient::TakeCopyOutputRequests(
    const LocalSurfaceId& latest_surface_id) {
  return std::vector<std::unique_ptr<CopyOutputRequest>>();
}

}  // namespace viz
