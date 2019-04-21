// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_WINDOW_OCCLUSION_CHANGE_BUILDER_H_
#define SERVICES_WS_WINDOW_OCCLUSION_CHANGE_BUILDER_H_

#include <memory>

#include "base/macros.h"
#include "ui/aura/window_occlusion_change_builder.h"
#include "ui/aura/window_tracker.h"

namespace ws {

// Group and send occlusion change to corresponding WindowTree in batches.
class WindowOcclusionChangeBuilder : public aura::WindowOcclusionChangeBuilder {
 public:
  WindowOcclusionChangeBuilder();
  ~WindowOcclusionChangeBuilder() override;

 private:
  // WindowOcclusionChangeBuilder:
  void Add(aura::Window* window,
           aura::Window::OcclusionState occlusion_state,
           SkRegion occluded_region) override;

  // Inner builder to apply the changes to windows at Window Service side.
  std::unique_ptr<aura::WindowOcclusionChangeBuilder> inner_;

  // Tracks live windows that has a change.
  aura::WindowTracker windows_;

  DISALLOW_COPY_AND_ASSIGN(WindowOcclusionChangeBuilder);
};

}  // namespace ws

#endif  // SERVICES_WS_WINDOW_OCCLUSION_CHANGE_BUILDER_H_
