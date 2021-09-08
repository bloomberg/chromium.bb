// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_session_focus_cycler.h"

#include "ash/capture_mode/capture_label_view.h"
#include "ash/capture_mode/capture_mode_bar_view.h"
#include "ash/capture_mode/capture_mode_button.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_session.h"
#include "ash/capture_mode/capture_mode_settings_entry_view.h"
#include "ash/capture_mode/capture_mode_settings_view.h"
#include "ash/capture_mode/capture_mode_source_view.h"
#include "ash/capture_mode/capture_mode_toggle_button.h"
#include "ash/capture_mode/capture_mode_type_view.h"
#include "ash/style/ash_color_provider.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// The focusable items for the FocusGroup::kSelection group.
constexpr std::array<FineTunePosition, 9> kSelectionTabbingOrder = {
    FineTunePosition::kCenter,       FineTunePosition::kTopLeft,
    FineTunePosition::kTopCenter,    FineTunePosition::kTopRight,
    FineTunePosition::kRightCenter,  FineTunePosition::kBottomRight,
    FineTunePosition::kBottomCenter, FineTunePosition::kBottomLeft,
    FineTunePosition::kLeftCenter};

}  // namespace

// -----------------------------------------------------------------------------
// CaptureModeSessionFocusCycler::HighlightableView:

std::unique_ptr<views::HighlightPathGenerator>
CaptureModeSessionFocusCycler::HighlightableView::CreatePathGenerator() {
  return nullptr;
}

void CaptureModeSessionFocusCycler::HighlightableView::PseudoFocus() {
  has_focus_ = true;

  views::View* view = GetView();
  DCHECK(view);

  if (!focus_ring_) {
    focus_ring_ = views::FocusRing::Install(view);
    focus_ring_->SetColor(AshColorProvider::Get()->GetControlsLayerColor(
        AshColorProvider::ControlsLayerType::kFocusRingColor));
    // Use a custom focus predicate as the default one checks if |view| actually
    // has focus which won't be happening since our widgets are not activatable.
    focus_ring_->SetHasFocusPredicate(
        [&](views::View* view) { return view->GetVisible() && has_focus_; });

    auto path_generator = CreatePathGenerator();
    if (path_generator)
      focus_ring_->SetPathGenerator(std::move(path_generator));
  }

  focus_ring_->Layout();
  focus_ring_->SchedulePaint();

  view->NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);
}

void CaptureModeSessionFocusCycler::HighlightableView::PseudoBlur() {
  has_focus_ = false;

  if (!focus_ring_)
    return;

  focus_ring_->Layout();
  focus_ring_->SchedulePaint();
}

void CaptureModeSessionFocusCycler::HighlightableView::ClickView() {
  views::View* view = GetView();
  DCHECK(view);

  views::Button* button = views::Button::AsButton(view);
  if (!button)
    return;
  button->AcceleratorPressed(ui::Accelerator(ui::VKEY_SPACE, /*modifiers=*/0));
}

// -----------------------------------------------------------------------------
// CaptureModeSessionFocusCycler:

CaptureModeSessionFocusCycler::CaptureModeSessionFocusCycler(
    CaptureModeSession* session)
    : session_(session) {}

CaptureModeSessionFocusCycler::~CaptureModeSessionFocusCycler() = default;

void CaptureModeSessionFocusCycler::AdvanceFocus(bool reverse) {
  // Advancing focus while the settings menu is open will close the menu and
  // clear focus, unless the settings menu was opened using keyboard navigation.
  if (!settings_menu_opened_with_keyboard_nav_) {
    views::Widget* settings_widget = GetSettingsMenuWidget();
    if (settings_widget && settings_widget->IsVisible()) {
      session_->SetSettingsMenuShown(false);
      return;
    }
  }

  ClearCurrentVisibleFocus();

  FocusGroup previous_focus_group = current_focus_group_;
  const size_t previous_group_size = GetGroupSize(previous_focus_group);
  const size_t previous_focus_index = focus_index_;

  // Go to the next group if the next index is out of bounds for the current
  // group. Otherwise, update |focus_index_| depending on |reverse|.
  if (!reverse && (previous_group_size == 0u ||
                   previous_focus_index == previous_group_size - 1u)) {
    current_focus_group_ = GetNextGroup(/*reverse=*/false);
    focus_index_ = 0u;
  } else if (reverse && previous_focus_index == 0u) {
    current_focus_group_ = GetNextGroup(/*reverse=*/true);
    focus_index_ = GetGroupSize(current_focus_group_) - 1u;
  } else {
    focus_index_ = reverse ? focus_index_ - 1u : focus_index_ + 1u;
  }

  // Focus the new item.
  std::vector<HighlightableView*> current_views =
      GetGroupItems(current_focus_group_);
  if (!current_views.empty()) {
    DCHECK_LT(focus_index_, current_views.size());
    current_views[focus_index_]->PseudoFocus();
  }

  // Selection focus is drawn directly on a layer owned by |session_|. Notify
  // the layer to repaint if necessary.
  const bool redraw_layer = previous_focus_group == FocusGroup::kSelection ||
                            current_focus_group_ == FocusGroup::kSelection;
  if (redraw_layer)
    session_->RepaintRegion();
}

void CaptureModeSessionFocusCycler::ClearFocus() {
  ClearCurrentVisibleFocus();

  if (current_focus_group_ == FocusGroup::kSelection)
    session_->RepaintRegion();

  current_focus_group_ = FocusGroup::kNone;
  focus_index_ = 0u;
}

bool CaptureModeSessionFocusCycler::HasFocus() const {
  return current_focus_group_ != FocusGroup::kNone;
}

bool CaptureModeSessionFocusCycler::OnSpacePressed() {
  if (current_focus_group_ == FocusGroup::kNone ||
      current_focus_group_ == FocusGroup::kSelection ||
      current_focus_group_ == FocusGroup::kPendingSettings) {
    return false;
  }

  std::vector<HighlightableView*> views = GetGroupItems(current_focus_group_);
  DCHECK(!views.empty());
  DCHECK_LT(focus_index_, views.size());
  HighlightableView* view = views[focus_index_];

  // Let the session handle the space key event if the region toggle button
  // currently has focus and we are already in region mode, as we still want to
  // create a default region in this case.
  CaptureModeBarView* bar_view = session_->capture_mode_bar_view_;
  if (view->GetView() ==
          bar_view->capture_source_view()->region_toggle_button() &&
      CaptureModeController::Get()->source() == CaptureModeSource::kRegion) {
    return false;
  }

  // Clicking on the settings button first clears current focus and moves us to
  // a temporary state. The next focus signal will navigate through the settings
  // items.
  if (view->GetView() == session_->capture_mode_bar_view_->settings_button()) {
    settings_menu_opened_with_keyboard_nav_ = true;
    ClearCurrentVisibleFocus();
    current_focus_group_ = FocusGroup::kPendingSettings;
    focus_index_ = 0u;
  }

  // ClickView comes last as it will destroy |this| if |view| is the close
  // button.
  view->ClickView();
  return true;
}

bool CaptureModeSessionFocusCycler::RegionGroupFocused() const {
  return current_focus_group_ == FocusGroup::kSelection ||
         current_focus_group_ == FocusGroup::kCaptureButton;
}

FineTunePosition CaptureModeSessionFocusCycler::GetFocusedFineTunePosition()
    const {
  if (current_focus_group_ != FocusGroup::kSelection)
    return FineTunePosition::kNone;
  return kSelectionTabbingOrder[focus_index_];
}

void CaptureModeSessionFocusCycler::OnCaptureLabelWidgetUpdated() {
  UpdateA11yAnnotation();
}

void CaptureModeSessionFocusCycler::OnSettingsMenuWidgetCreated() {
  views::Widget* settings_menu_widget =
      session_->capture_mode_settings_widget_.get();
  DCHECK(settings_menu_widget);
  settings_menu_widget_observeration_.Observe(settings_menu_widget);
  UpdateA11yAnnotation();
}

void CaptureModeSessionFocusCycler::OnWidgetClosing(views::Widget* widget) {
  // Remove focus if one of the settings related groups is currently focused.
  if (current_focus_group_ == FocusGroup::kPendingSettings ||
      current_focus_group_ == FocusGroup::kSettingsMenu) {
    ClearFocus();
  }

  settings_menu_opened_with_keyboard_nav_ = false;
  settings_menu_widget_observeration_.Reset();
  UpdateA11yAnnotation();
}

void CaptureModeSessionFocusCycler::ClearCurrentVisibleFocus() {
  // The settings menu widget may be destroyed while it has focus. No need to
  // clear focus in this case.
  if (current_focus_group_ == FocusGroup::kSettingsMenu &&
      !GetSettingsMenuWidget()) {
    return;
  }

  std::vector<HighlightableView*> views = GetGroupItems(current_focus_group_);
  if (!views.empty()) {
    DCHECK_LT(focus_index_, views.size());
    views[focus_index_]->PseudoBlur();
  }
}

CaptureModeSessionFocusCycler::FocusGroup
CaptureModeSessionFocusCycler::GetNextGroup(bool reverse) const {
  // The selection UI and capture button are focusable only when the label
  // button of CaptureLabelView is visible.
  bool selection_available = false;
  views::Widget* widget = session_->capture_label_widget_.get();
  if (widget) {
    auto* capture_label_view =
        static_cast<CaptureLabelView*>(widget->GetContentsView());
    selection_available = capture_label_view->label_button()->GetVisible();
  }

  switch (current_focus_group_) {
    case FocusGroup::kNone:
      return reverse ? FocusGroup::kSettingsClose : FocusGroup::kTypeSource;
    case FocusGroup::kTypeSource:
      if (reverse)
        return FocusGroup::kSettingsClose;
      return selection_available ? FocusGroup::kSelection
                                 : FocusGroup::kSettingsClose;
    case FocusGroup::kSelection:
      DCHECK(selection_available);
      return reverse ? FocusGroup::kTypeSource : FocusGroup::kCaptureButton;
    case FocusGroup::kCaptureButton:
      DCHECK(selection_available);
      return reverse ? FocusGroup::kSelection : FocusGroup::kSettingsClose;
    case FocusGroup::kSettingsClose:
      if (!reverse)
        return FocusGroup::kTypeSource;
      return selection_available ? FocusGroup::kCaptureButton
                                 : FocusGroup::kTypeSource;
    case FocusGroup::kPendingSettings:
      DCHECK(GetSettingsMenuWidget());
      return FocusGroup::kSettingsMenu;
    case FocusGroup::kSettingsMenu:
      return reverse ? FocusGroup::kTypeSource : FocusGroup::kSettingsClose;
  }

  NOTREACHED();
  return FocusGroup::kNone;
}

size_t CaptureModeSessionFocusCycler::GetGroupSize(FocusGroup group) const {
  if (group == FocusGroup::kSelection)
    return 9u;
  return GetGroupItems(group).size();
}

std::vector<CaptureModeSessionFocusCycler::HighlightableView*>
CaptureModeSessionFocusCycler::GetGroupItems(FocusGroup group) const {
  std::vector<HighlightableView*> items;
  switch (group) {
    case FocusGroup::kNone:
    case FocusGroup::kSelection:
    case FocusGroup::kPendingSettings:
      break;
    case FocusGroup::kTypeSource: {
      CaptureModeBarView* bar_view = session_->capture_mode_bar_view_;
      CaptureModeTypeView* type_view = bar_view->capture_type_view();
      CaptureModeSourceView* source_view = bar_view->capture_source_view();
      items = {type_view->image_toggle_button(),
               type_view->video_toggle_button(),
               source_view->fullscreen_toggle_button(),
               source_view->region_toggle_button(),
               source_view->window_toggle_button()};
      break;
    }
    case FocusGroup::kCaptureButton: {
      views::Widget* widget = session_->capture_label_widget_.get();
      DCHECK(widget);
      items = {static_cast<CaptureLabelView*>(widget->GetContentsView())};
      break;
    }
    case FocusGroup::kSettingsClose: {
      CaptureModeBarView* bar_view = session_->capture_mode_bar_view_;
      items = {bar_view->settings_button(), bar_view->close_button()};
      break;
    }
    case FocusGroup::kSettingsMenu: {
      CaptureModeSettingsView* settings_view =
          session_->capture_mode_settings_view_;
      DCHECK(settings_view);
      items = {settings_view->microphone_view()};
      break;
    }
  }

  return items;
}

views::Widget* CaptureModeSessionFocusCycler::GetSettingsMenuWidget() const {
  return session_->capture_mode_settings_widget_.get();
}

void CaptureModeSessionFocusCycler::UpdateA11yAnnotation() {
  std::vector<views::Widget*> a11y_widgets;

  // If the bar widget is not available, then this is called while shutting
  // down the capture mode session.
  views::Widget* bar_widget = session_->capture_mode_bar_widget_.get();
  if (bar_widget)
    a11y_widgets.push_back(bar_widget);

  // Add the label widget only if the button is visible.
  views::Widget* label_widget = session_->capture_label_widget_.get();
  if (label_widget &&
      static_cast<CaptureLabelView*>(label_widget->GetContentsView())
          ->label_button()
          ->GetVisible()) {
    a11y_widgets.push_back(label_widget);
  }

  // Add the settings widget if it exists.
  views::Widget* settings_menu_widget =
      session_->capture_mode_settings_widget_.get();
  if (settings_menu_widget)
    a11y_widgets.push_back(settings_menu_widget);

  // Helper to update |target|'s a11y focus with |previous| and |next|, which
  // can be null.
  auto update_a11y_widget_focus =
      [](views::Widget* target, views::Widget* previous, views::Widget* next) {
        DCHECK(target);
        auto* contents_view = target->GetContentsView();
        auto& view_a11y = contents_view->GetViewAccessibility();
        view_a11y.OverridePreviousFocus(previous);
        view_a11y.OverrideNextFocus(next);
        contents_view->NotifyAccessibilityEvent(ax::mojom::Event::kTreeChanged,
                                                true);
      };

  // If there is only one widget left, clear the focus overrides so that they
  // do not point to deleted objects.
  if (a11y_widgets.size() == 1u) {
    update_a11y_widget_focus(a11y_widgets[0], nullptr, nullptr);
    return;
  }

  const int size = a11y_widgets.size();
  for (int i = 0; i < size; ++i) {
    const int previous_index = (i + size - 1) % size;
    const int next_index = (i + 1) % size;
    update_a11y_widget_focus(a11y_widgets[i], a11y_widgets[previous_index],
                             a11y_widgets[next_index]);
  }
}

}  // namespace ash
