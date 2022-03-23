// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/input_mapping_view.h"

#include "ui/views/background.h"

namespace arc {
namespace input_overlay {
namespace {
// UI specs.
constexpr SkColor kEditModeBgColor = SkColorSetA(SK_ColorGRAY, 0x99);
}  // namespace

InputMappingView::InputMappingView(
    DisplayOverlayController* display_overlay_controller)
    : display_overlay_controller_(display_overlay_controller) {
  auto content_bounds = input_overlay::CalculateWindowContentBounds(
      display_overlay_controller_->touch_injector()->target_window());
  auto& actions = display_overlay_controller_->touch_injector()->actions();
  SetBounds(content_bounds.x(), content_bounds.y(), content_bounds.width(),
            content_bounds.height());
  for (auto& action : actions) {
    auto view = action->CreateView(display_overlay_controller_, content_bounds);
    if (view)
      AddChildView(std::move(view));
  }
}

InputMappingView::~InputMappingView() = default;

void InputMappingView::SetDisplayMode(const DisplayMode mode) {
  if (current_display_mode_ == mode)
    return;
  switch (mode) {
    case DisplayMode::kMenu:
    case DisplayMode::kView:
      SetBackground(nullptr);
      break;
    case DisplayMode::kEdit:
      SetBackground(views::CreateSolidBackground(kEditModeBgColor));
      break;
    default:
      NOTREACHED();
      break;
  }
  for (auto* view : children()) {
    auto* action_view = static_cast<ActionView*>(view);
    action_view->SetDisplayMode(mode);
  }
  current_display_mode_ = mode;
}

}  // namespace input_overlay
}  // namespace arc
