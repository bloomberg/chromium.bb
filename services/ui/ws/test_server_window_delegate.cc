// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/test_server_window_delegate.h"

#include "services/ui/ws/server_window.h"

namespace ui {
namespace ws {

TestServerWindowDelegate::TestServerWindowDelegate(
    viz::HostFrameSinkManager* host_frame_sink_manager)
    : host_frame_sink_manager_(host_frame_sink_manager) {}

TestServerWindowDelegate::~TestServerWindowDelegate() {}

void TestServerWindowDelegate::AddRootWindow(ServerWindow* window) {
  roots_.insert(window);
}

viz::HostFrameSinkManager* TestServerWindowDelegate::GetHostFrameSinkManager() {
  return host_frame_sink_manager_;
}

ServerWindow* TestServerWindowDelegate::GetRootWindowForDrawn(
    const ServerWindow* window) {
  for (ServerWindow* root : roots_) {
    if (root->Contains(window))
      return root;
  }
  return root_window_;
}

void TestServerWindowDelegate::OnFirstSurfaceActivation(
    const viz::SurfaceInfo& surface_info,
    ServerWindow* window) {}

}  // namespace ws
}  // namespace ui
