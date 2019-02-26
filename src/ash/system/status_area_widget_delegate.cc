// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/status_area_widget_delegate.h"

#include "ash/focus_cycler.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/tray_constants.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/border.h"
#include "ui/views/layout/grid_layout.h"

namespace {

using session_manager::SessionState;

constexpr int kAnimationDurationMs = 250;

constexpr int kPaddingBetweenWidgetsNewUi = 8;

class StatusAreaWidgetDelegateAnimationSettings
    : public ui::ScopedLayerAnimationSettings {
 public:
  explicit StatusAreaWidgetDelegateAnimationSettings(ui::Layer* layer)
      : ui::ScopedLayerAnimationSettings(layer->GetAnimator()) {
    SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kAnimationDurationMs));
    SetPreemptionStrategy(ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    SetTweenType(gfx::Tween::EASE_IN_OUT);
  }

  ~StatusAreaWidgetDelegateAnimationSettings() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(StatusAreaWidgetDelegateAnimationSettings);
};

}  // namespace

namespace ash {

StatusAreaWidgetDelegate::StatusAreaWidgetDelegate(Shelf* shelf)
    : shelf_(shelf), focus_cycler_for_testing_(nullptr) {
  DCHECK(shelf_);
  set_owned_by_client();  // Deleted by DeleteDelegate().

  // Allow the launcher to surrender the focus to another window upon
  // navigation completion by the user.
  set_allow_deactivate_on_esc(true);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

StatusAreaWidgetDelegate::~StatusAreaWidgetDelegate() = default;

void StatusAreaWidgetDelegate::SetFocusCyclerForTesting(
    const FocusCycler* focus_cycler) {
  focus_cycler_for_testing_ = focus_cycler;
}

bool StatusAreaWidgetDelegate::ShouldFocusOut(bool reverse) {
  if (Shell::Get()->session_controller()->GetSessionState() ==
      SessionState::ACTIVE) {
    return false;
  }

  views::View* focused_view = GetFocusManager()->GetFocusedView();
  return (reverse && focused_view == GetFirstFocusableChild()) ||
         (!reverse && focused_view == GetLastFocusableChild());
}

views::View* StatusAreaWidgetDelegate::GetDefaultFocusableChild() {
  return default_last_focusable_child_ ? GetLastFocusableChild()
                                       : GetFirstFocusableChild();
}

views::Widget* StatusAreaWidgetDelegate::GetWidget() {
  return View::GetWidget();
}

const views::Widget* StatusAreaWidgetDelegate::GetWidget() const {
  return View::GetWidget();
}

void StatusAreaWidgetDelegate::OnGestureEvent(ui::GestureEvent* event) {
  views::Widget* target_widget =
      static_cast<views::View*>(event->target())->GetWidget();
  Shelf* shelf = Shelf::ForWindow(target_widget->GetNativeWindow());

  // Convert the event location from current view to screen, since swiping up on
  // the shelf can open the fullscreen app list. Updating the bounds of the app
  // list during dragging is based on screen coordinate space.
  ui::GestureEvent event_in_screen(*event);
  gfx::Point location_in_screen(event->location());
  View::ConvertPointToScreen(this, &location_in_screen);
  event_in_screen.set_location(location_in_screen);
  if (shelf->ProcessGestureEvent(event_in_screen))
    event->StopPropagation();
  else
    views::AccessiblePaneView::OnGestureEvent(event);
}

bool StatusAreaWidgetDelegate::CanActivate() const {
  // We don't want mouse clicks to activate us, but we need to allow
  // activation when the user is using the keyboard (FocusCycler).
  const FocusCycler* focus_cycler = focus_cycler_for_testing_
                                        ? focus_cycler_for_testing_
                                        : Shell::Get()->focus_cycler();
  return focus_cycler->widget_activating() == GetWidget();
}

void StatusAreaWidgetDelegate::DeleteDelegate() {
  delete this;
}

void StatusAreaWidgetDelegate::UpdateLayout() {
  // Use a grid layout so that the trays can be centered in each cell, and
  // so that the widget gets laid out correctly when tray sizes change.
  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>(this));

  // Update tray border based on layout.
  bool is_child_on_edge = true;
  for (int c = 0; c < child_count(); ++c) {
    views::View* child = child_at(c);
    if (!child->visible())
      continue;
    SetBorderOnChild(child, is_child_on_edge);
    is_child_on_edge = false;
  }

  views::ColumnSet* columns = layout->AddColumnSet(0);

  if (shelf_->IsHorizontalAlignment()) {
    for (int c = child_count() - 1; c >= 0; --c) {
      views::View* child = child_at(c);
      if (!child->visible())
        continue;
      columns->AddColumn(views::GridLayout::CENTER, views::GridLayout::FILL,
                         0, /* resize percent */
                         views::GridLayout::USE_PREF, 0, 0);
    }
    layout->StartRow(0, 0);
    for (int c = child_count() - 1; c >= 0; --c) {
      views::View* child = child_at(c);
      if (child->visible())
        layout->AddView(child);
    }
  } else {
    columns->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                       0, /* resize percent */
                       views::GridLayout::USE_PREF, 0, 0);
    for (int c = child_count() - 1; c >= 0; --c) {
      views::View* child = child_at(c);
      if (!child->visible())
        continue;
      layout->StartRow(0, 0);
      layout->AddView(child);
    }
  }

  layer()->GetAnimator()->StopAnimating();
  StatusAreaWidgetDelegateAnimationSettings settings(layer());

  Layout();
  UpdateWidgetSize();
}

void StatusAreaWidgetDelegate::ChildPreferredSizeChanged(View* child) {
  // Need to resize the window when trays or items are added/removed.
  StatusAreaWidgetDelegateAnimationSettings settings(layer());
  UpdateWidgetSize();
}

void StatusAreaWidgetDelegate::ChildVisibilityChanged(View* child) {
  UpdateLayout();
}

void StatusAreaWidgetDelegate::UpdateWidgetSize() {
  if (GetWidget())
    GetWidget()->SetSize(GetPreferredSize());
}

void StatusAreaWidgetDelegate::SetBorderOnChild(views::View* child,
                                                bool extend_border_to_edge) {
  const int vertical_padding =
      (ShelfConstants::shelf_size() - kTrayItemSize) / 2;

  // Edges for horizontal alignment (right-to-left, default).
  int top_edge = vertical_padding;
  int left_edge = 0;
  int bottom_edge = vertical_padding;
  // Add some extra space so that borders don't overlap. This padding between
  // items also takes care of padding at the edge of the shelf.
  int right_edge = kPaddingBetweenWidgetsNewUi;

  // Swap edges if alignment is not horizontal (bottom-to-top).
  if (!shelf_->IsHorizontalAlignment()) {
    std::swap(top_edge, left_edge);
    std::swap(bottom_edge, right_edge);
  }

  child->SetBorder(
      views::CreateEmptyBorder(top_edge, left_edge, bottom_edge, right_edge));

  // Layout on |child| needs to be updated based on new border value before
  // displaying; otherwise |child| will be showing with old border size.
  // Fix for crbug.com/623438.
  child->Layout();
}

}  // namespace ash
