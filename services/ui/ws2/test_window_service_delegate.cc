// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws2/test_window_service_delegate.h"

#include "ui/aura/window.h"

namespace ui {
namespace ws2 {

TestWindowServiceDelegate::TestWindowServiceDelegate(
    aura::Window* top_level_parent)
    : top_level_parent_(top_level_parent) {}

TestWindowServiceDelegate::~TestWindowServiceDelegate() = default;

std::unique_ptr<aura::Window> TestWindowServiceDelegate::NewTopLevel(
    const base::flat_map<std::string, std::vector<uint8_t>>& properties) {
  std::unique_ptr<aura::Window> window =
      std::make_unique<aura::Window>(nullptr);
  window->Init(LAYER_NOT_DRAWN);
  if (top_level_parent_)
    top_level_parent_->AddChild(window.get());
  return window;
}

}  // namespace ws2
}  // namespace ui
