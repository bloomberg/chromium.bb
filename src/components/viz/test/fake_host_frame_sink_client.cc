// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/fake_host_frame_sink_client.h"

namespace viz {

FakeHostFrameSinkClient::FakeHostFrameSinkClient() = default;

FakeHostFrameSinkClient::~FakeHostFrameSinkClient() = default;

void FakeHostFrameSinkClient::OnFrameTokenChanged(uint32_t frame_token) {
  last_frame_token_seen_ = frame_token;
}

}  // namespace viz
