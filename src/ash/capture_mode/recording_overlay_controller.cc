// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/recording_overlay_controller.h"

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/projector/projector_annotation_tray.h"
#include "ash/public/cpp/capture_mode/recording_overlay_view.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/system/status_area_widget.h"
#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// When recording a non-root window (i.e. kWindow recording source), the overlay
// is added as a direct child of the window being recorded, and stacked on top
// of all children. This is so that the overlay contents show up in the
// recording above everything else.
//
//   + window_being_recorded
//       |
//       + (Some other child windows hosting contents of the window)
//       |
//       + Recording overlay widget
//
// (Note that bottom-most child are the top-most child in terms of z-order).
//
// However, when recording the root window (i.e. kFullscreen or kRegion
// recording sources), the overlay is added as a child of the menu container.
// The menu container is high enough in terms of z-order, making the overlay on
// top of most things. However, it's also the same container used by the
// projector bar (which we want to be on top of the overlay, since it has the
// button to toggle the overlay off, and we don't want the overlay to block
// events going to that button). Therefore, the overlay is stacked at the bottom
// of the menu container's children. See UpdateWidgetStacking() below.
//
//   + Menu container
//     |
//     + Recording overlay widget
//     |
//     + Projector bar widget
//
// TODO(https://crbug.com/1253011): Revise this parenting and z-ordering once
// the deprecated Projector toolbar is removed and replaced by the shelf-pod
// based new tools.
aura::Window* GetWidgetParent(aura::Window* window_being_recorded) {
  return window_being_recorded->IsRootWindow()
             ? window_being_recorded->GetChildById(kShellWindowId_MenuContainer)
             : window_being_recorded;
}

// Defines a window targeter that will be installed on the overlay widget's
// window so that we can allow located events over the projector shelf pod or
// its associated bubble widget to go through and not be consumed by the
// overlay. This enables the user to interact with the annotation tools while
// annotating.
class OverlayTargeter : public aura::WindowTargeter {
 public:
  explicit OverlayTargeter(aura::Window* overlay_window)
      : overlay_window_(overlay_window) {}
  OverlayTargeter(const OverlayTargeter&) = delete;
  OverlayTargeter& operator=(const OverlayTargeter&) = delete;
  ~OverlayTargeter() override = default;

  // aura::WindowTargeter:
  ui::EventTarget* FindTargetForEvent(ui::EventTarget* root,
                                      ui::Event* event) override {
    if (event->IsLocatedEvent()) {
      auto* root_window = overlay_window_->GetRootWindow();
      ProjectorAnnotationTray* annotations =
          RootWindowController::ForWindow(root_window)
              ->GetStatusAreaWidget()
              ->projector_annotation_tray();
      if (annotations && annotations->visible_preferred()) {
        auto* located_event = event->AsLocatedEvent();
        auto screen_location = located_event->root_location();
        wm::ConvertPointToScreen(root_window, &screen_location);

        // Let events over the projector shelf pod to go through.
        if (annotations->GetBoundsInScreen().Contains(screen_location))
          return nullptr;

        // Let events over the projector bubble widget (if shown) to go through.
        views::Widget* bubble_widget = annotations->GetBubbleWidget();
        if (bubble_widget && bubble_widget->IsVisible() &&
            bubble_widget->GetWindowBoundsInScreen().Contains(
                screen_location)) {
          return nullptr;
        }
      }
    }

    return aura::WindowTargeter::FindTargetForEvent(root, event);
  }

 private:
  aura::Window* const overlay_window_;
};

}  // namespace

RecordingOverlayController::RecordingOverlayController(
    aura::Window* window_being_recorded,
    const gfx::Rect& initial_bounds_in_parent) {
  DCHECK(window_being_recorded);
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.name = "RecordingOverlayWidget";
  params.child = true;
  params.parent = GetWidgetParent(window_being_recorded);
  // The overlay hosts transparent contents so actual contents of the window
  // being recorded shows up underneath.
  params.layer_type = ui::LAYER_NOT_DRAWN;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.bounds = initial_bounds_in_parent;
  // The overlay window does not receive any events until it's shown and
  // enabled. See |Start()| below.
  params.activatable = views::Widget::InitParams::Activatable::kNo;
  params.accept_events = false;
  overlay_widget_->Init(std::move(params));
  recording_overlay_view_ = overlay_widget_->SetContentsView(
      CaptureModeController::Get()->CreateRecordingOverlayView());
  auto* overlay_window = overlay_widget_->GetNativeWindow();
  overlay_window->SetEventTargeter(
      std::make_unique<OverlayTargeter>(overlay_window));
  UpdateWidgetStacking();
}

void RecordingOverlayController::Toggle() {
  is_enabled_ = !is_enabled_;
  if (is_enabled_)
    Start();
  else
    Stop();
}

void RecordingOverlayController::SetBounds(const gfx::Rect& bounds_in_parent) {
  overlay_widget_->SetBounds(bounds_in_parent);
}

aura::Window* RecordingOverlayController::GetOverlayNativeWindow() {
  return overlay_widget_->GetNativeWindow();
}

void RecordingOverlayController::Start() {
  DCHECK(is_enabled_);

  overlay_widget_->GetNativeWindow()->SetEventTargetingPolicy(
      aura::EventTargetingPolicy::kTargetAndDescendants);
  overlay_widget_->Show();
}

void RecordingOverlayController::Stop() {
  DCHECK(!is_enabled_);

  overlay_widget_->GetNativeWindow()->SetEventTargetingPolicy(
      aura::EventTargetingPolicy::kNone);
  overlay_widget_->Hide();
}

void RecordingOverlayController::UpdateWidgetStacking() {
  auto* overlay_window = overlay_widget_->GetNativeWindow();
  auto* parent = overlay_window->parent();
  DCHECK(parent);

  // See more info in the docs of GetWidgetParent() above.
  if (parent->GetId() == kShellWindowId_MenuContainer)
    parent->StackChildAtBottom(overlay_window);
  else
    parent->StackChildAtTop(overlay_window);
}

}  // namespace ash
