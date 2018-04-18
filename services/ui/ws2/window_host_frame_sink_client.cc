// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws2/window_host_frame_sink_client.h"

#include "base/logging.h"
#include "services/ui/ws2/window_data.h"
#include "ui/aura/mus/client_surface_embedder.h"

namespace ui {
namespace ws2 {

WindowHostFrameSinkClient::WindowHostFrameSinkClient() = default;

WindowHostFrameSinkClient::~WindowHostFrameSinkClient() {}

void WindowHostFrameSinkClient::OnFrameSinkIdChanged() {}

void WindowHostFrameSinkClient::OnFirstSurfaceActivation(
    const viz::SurfaceInfo& surface_info) {
  NOTIMPLEMENTED();
}

void WindowHostFrameSinkClient::OnFrameTokenChanged(uint32_t frame_token) {
  NOTIMPLEMENTED();
}

}  // namespace ws2
}  // namespace ui
