// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/surfaces/surfaces_state.h"
#include "services/ui/ws/server_window.h"
#include "services/ui/ws/test_server_window_delegate.h"

namespace ui {

namespace ws {

TestServerWindowDelegate::TestServerWindowDelegate()
    : root_window_(nullptr), surfaces_state_(new SurfacesState()) {}

TestServerWindowDelegate::~TestServerWindowDelegate() {}

ui::SurfacesState* TestServerWindowDelegate::GetSurfacesState() {
  return surfaces_state_.get();
}

void TestServerWindowDelegate::OnScheduleWindowPaint(ServerWindow* window) {}

const ServerWindow* TestServerWindowDelegate::GetRootWindow(
    const ServerWindow* window) const {
  return root_window_;
}

void TestServerWindowDelegate::ScheduleSurfaceDestruction(
    ServerWindow* window) {}

}  // namespace ws

}  // namespace ui
