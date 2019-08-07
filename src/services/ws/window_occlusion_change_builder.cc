// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/window_occlusion_change_builder.h"

#include <map>
#include <set>

#include "services/ws/proxy_window.h"
#include "services/ws/window_tree.h"

namespace ws {

WindowOcclusionChangeBuilder::WindowOcclusionChangeBuilder()
    : inner_(aura::WindowOcclusionChangeBuilder::Create()) {}

WindowOcclusionChangeBuilder::~WindowOcclusionChangeBuilder() {
  // Apply occlusion changes.
  inner_.reset();

  // Get remote windows and group them based on owning trees.
  std::map<WindowTree*, std::set<aura::Window*>> changes;
  while (!windows_.windows().empty()) {
    aura::Window* window = windows_.Pop();

    ProxyWindow* const proxy_window = ProxyWindow::GetMayBeNull(window);
    if (!proxy_window || !proxy_window->owning_window_tree())
      continue;

    changes[proxy_window->owning_window_tree()].insert(window);
  }

  // Send out changes on a per-tree basis.
  for (auto& change : changes)
    change.first->SendOcclusionStates(change.second);
}

void WindowOcclusionChangeBuilder::Add(
    aura::Window* window,
    aura::Window::OcclusionState occlusion_state,
    SkRegion occluded_region) {
  windows_.Add(window);
  inner_->Add(window, occlusion_state, occluded_region);
}

}  // namespace ws
