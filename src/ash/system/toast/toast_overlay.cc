// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/toast/toast_overlay.h"

#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/ash_typography.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/pill_button.h"
#include "ash/style/system_toast_style.h"
#include "ash/wm/work_area_insets.h"
#include "base/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/window_animations.h"

namespace ash {

namespace {

// Duration of slide animation when overlay is shown or hidden.
constexpr int kSlideAnimationDurationMs = 100;

// Returns the work area bounds for the root window where new windows are added
// (including new toasts).
gfx::Rect GetUserWorkAreaBounds(aura::Window* window) {
  return WorkAreaInsets::ForWindow(window)->user_work_area_bounds();
}

// Offsets the bottom of bounds for toast to accommodate the hotseat, based on
// the current hotseat state
void AdjustWorkAreaBoundsForHotseatState(gfx::Rect& bounds,
                                         const HotseatWidget* hotseat_widget) {
  if (hotseat_widget->state() == HotseatState::kExtended)
    bounds.set_height(bounds.height() - hotseat_widget->GetHotseatSize());
  if (hotseat_widget->state() == HotseatState::kShownHomeLauncher)
    bounds.set_height(hotseat_widget->GetTargetBounds().y() - bounds.y());
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//  ToastDisplayObserver
class ToastOverlay::ToastDisplayObserver : public display::DisplayObserver {
 public:
  ToastDisplayObserver(ToastOverlay* overlay) : overlay_(overlay) {}

  ToastDisplayObserver(const ToastDisplayObserver&) = delete;
  ToastDisplayObserver& operator=(const ToastDisplayObserver&) = delete;

  ~ToastDisplayObserver() override {}

  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override {
    overlay_->UpdateOverlayBounds();
  }

 private:
  ToastOverlay* const overlay_;

  display::ScopedDisplayObserver display_observer_{this};
};

///////////////////////////////////////////////////////////////////////////////
//  ToastOverlay
ToastOverlay::ToastOverlay(Delegate* delegate,
                           const std::u16string& text,
                           const std::u16string& dismiss_text,
                           bool show_on_lock_screen,
                           bool is_managed,
                           base::RepeatingClosure dismiss_callback,
                           base::RepeatingClosure expired_callback)
    : delegate_(delegate),
      text_(text),
      dismiss_text_(dismiss_text),
      overlay_widget_(new views::Widget),
      overlay_view_(new SystemToastStyle(
          base::BindRepeating(&ToastOverlay::OnButtonClicked,
                              base::Unretained(this)),
          text,
          dismiss_text,
          is_managed)),
      display_observer_(std::make_unique<ToastDisplayObserver>(this)),
      dismiss_callback_(std::move(dismiss_callback)),
      expired_callback_(std::move(expired_callback)),
      widget_size_(overlay_view_->GetPreferredSize()) {
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.name = "ToastOverlay";
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.accept_events = true;
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  params.bounds = CalculateOverlayBounds();
  // Show toasts above the app list and below the lock screen.
  params.parent = Shell::GetRootWindowForNewWindows()->GetChildById(
      show_on_lock_screen ? kShellWindowId_LockSystemModalContainer
                          : kShellWindowId_SystemModalContainer);
  overlay_widget_->Init(std::move(params));
  overlay_widget_->SetVisibilityChangedAnimationsEnabled(true);
  overlay_widget_->SetContentsView(overlay_view_.get());
  UpdateOverlayBounds();

  aura::Window* overlay_window = overlay_widget_->GetNativeWindow();
  ::wm::SetWindowVisibilityAnimationType(
      overlay_window, ::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_VERTICAL);
  ::wm::SetWindowVisibilityAnimationDuration(
      overlay_window, base::Milliseconds(kSlideAnimationDurationMs));

  keyboard::KeyboardUIController::Get()->AddObserver(this);
}

ToastOverlay::~ToastOverlay() {
  keyboard::KeyboardUIController::Get()->RemoveObserver(this);
  overlay_widget_->Close();
  if (expired_callback_)
    expired_callback_.Run();
}

void ToastOverlay::Show(bool visible) {
  if (overlay_widget_->GetLayer()->GetTargetVisibility() == visible)
    return;

  ui::LayerAnimator* animator = overlay_widget_->GetLayer()->GetAnimator();
  DCHECK(animator);

  base::TimeDelta original_duration = animator->GetTransitionDuration();
  ui::ScopedLayerAnimationSettings animation_settings(animator);
  // ScopedLayerAnimationSettings ctor changes the transition duration, so
  // change it back to the original value (should be zero).
  animation_settings.SetTransitionDuration(original_duration);

  animation_settings.AddObserver(this);

  if (visible) {
    overlay_widget_->Show();

    // Notify accessibility about the overlay.
    overlay_view_->NotifyAccessibilityEvent(ax::mojom::Event::kAlert, false);
  } else {
    overlay_widget_->Hide();
  }
}

void ToastOverlay::UpdateOverlayBounds() {
  overlay_widget_->SetBounds(CalculateOverlayBounds());
}

const std::u16string ToastOverlay::GetText() {
  return text_;
}

bool ToastOverlay::MaybeToggleA11yHighlightOnDismissButton() {
  return overlay_view_->ToggleA11yFocus();
}

bool ToastOverlay::MaybeActivateHighlightedDismissButton() {
  if (!overlay_view_->is_dismiss_button_highlighted())
    return false;

  OnButtonClicked();
  return true;
}

gfx::Rect ToastOverlay::CalculateOverlayBounds() {
  // If the native window has not been initialized, as in the first call, get
  // the default root window. Otherwise get the window for this overlay_widget
  // to handle multiple monitors properly.
  auto* window = overlay_widget_->IsNativeWidgetInitialized()
                     ? overlay_widget_->GetNativeWindow()
                     : Shell::GetRootWindowForNewWindows();
  auto* window_controller = RootWindowController::ForWindow(window);
  auto* hotseat_widget = window_controller->shelf()->hotseat_widget();

  gfx::Rect bounds = GetUserWorkAreaBounds(window);

  if (hotseat_widget)
    AdjustWorkAreaBoundsForHotseatState(bounds, hotseat_widget);

  int target_y =
      bounds.bottom() - widget_size_.height() - ToastOverlay::kOffset;
  bounds.ClampToCenteredSize(widget_size_);
  bounds.set_y(target_y);
  return bounds;
}

void ToastOverlay::OnButtonClicked() {
  if (dismiss_callback_) {
    dismiss_callback_.Run();
  }
  Show(/*visible=*/false);
}

void ToastOverlay::OnImplicitAnimationsScheduled() {}

void ToastOverlay::OnImplicitAnimationsCompleted() {
  if (!overlay_widget_->GetLayer()->GetTargetVisibility())
    delegate_->OnClosed();
}

void ToastOverlay::OnKeyboardOccludedBoundsChanged(
    const gfx::Rect& new_bounds_in_screen) {
  // TODO(https://crbug.com/943446): Observe changes in user work area bounds
  // directly instead of listening for keyboard bounds changes.
  UpdateOverlayBounds();
}

views::Widget* ToastOverlay::widget_for_testing() {
  return overlay_widget_.get();
}

views::LabelButton* ToastOverlay::dismiss_button_for_testing() {
  return overlay_view_->button();
}

}  // namespace ash
