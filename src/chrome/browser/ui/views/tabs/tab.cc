// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab.h"

#include <stddef.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/debug/alias.h"
#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "base/metrics/user_metrics.h"
#include "base/numerics/ranges.h"
#include "base/scoped_observer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_recorder.h"
#include "cc/paint/paint_shader.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/alert_indicator.h"
#include "chrome/browser/ui/views/tabs/browser_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_close_button.h"
#include "chrome/browser/ui/views/tabs/tab_controller.h"
#include "chrome/browser/ui/views/tabs/tab_hover_card_bubble_view.h"
#include "chrome/browser/ui/views/tabs/tab_icon.h"
#include "chrome/browser/ui/views/tabs/tab_slot_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_layout.h"
#include "chrome/browser/ui/views/tabs/tab_style_views.h"
#include "chrome/browser/ui/views/touch_uma/touch_uma.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/grit/components_scaled_resources.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "third_party/skia/include/pathops/SkPathOps.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/compositor/clip_recorder.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skia_util.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/rect_based_targeting_utils.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_targeter.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

#if defined(USE_AURA)
#include "ui/aura/env.h"
#endif

using base::UserMetricsAction;
namespace {

// When a non-pinned tab becomes a pinned tab the width of the tab animates. If
// the width of a pinned tab is at least kPinnedTabExtraWidthToRenderAsNormal
// larger than the desired pinned tab width then the tab is rendered as a normal
// tab. This is done to avoid having the title immediately disappear when
// transitioning a tab from normal to pinned tab.
constexpr int kPinnedTabExtraWidthToRenderAsNormal = 30;

bool g_show_hover_card_on_mouse_hover = true;

// Helper functions ------------------------------------------------------------

// Returns the coordinate for an object of size |item_size| centered in a region
// of size |size|, biasing towards placing any extra space ahead of the object.
int Center(int size, int item_size) {
  int extra_space = size - item_size;
  // Integer division below truncates, thus effectively "rounding toward zero";
  // to always place extra space ahead of the object, we want to round towards
  // positive infinity, which means we need to bias the division only when the
  // size difference is positive.  (Adding one unconditionally will stack with
  // the truncation if |extra_space| is negative, resulting in off-by-one
  // errors.)
  if (extra_space > 0)
    ++extra_space;
  return extra_space / 2;
}

class TabStyleHighlightPathGenerator : public views::HighlightPathGenerator {
 public:
  explicit TabStyleHighlightPathGenerator(TabStyle* tab_style)
      : tab_style_(tab_style) {}

  // views::HighlightPathGenerator:
  SkPath GetHighlightPath(const views::View* view) override {
    return tab_style_->GetPath(TabStyle::PathType::kHighlight, 1.0);
  }

 private:
  TabStyle* const tab_style_;

  DISALLOW_COPY_AND_ASSIGN(TabStyleHighlightPathGenerator);
};

}  // namespace

// Helper class that observes the tab's close button.
class Tab::TabCloseButtonObserver : public views::ViewObserver {
 public:
  explicit TabCloseButtonObserver(Tab* tab,
                                  views::View* close_button,
                                  TabController* controller)
      : tab_(tab), close_button_(close_button), controller_(controller) {
    DCHECK(close_button_);
    tab_close_button_observer_.Add(close_button_);
  }

  ~TabCloseButtonObserver() override {
    tab_close_button_observer_.Remove(close_button_);
  }

 private:
  void OnViewFocused(views::View* observed_view) override {
    controller_->UpdateHoverCard(tab_);
  }

  void OnViewBlurred(views::View* observed_view) override {
    // Only hide hover card if not keyboard navigating.
    if (!controller_->IsFocusInTabs())
      controller_->UpdateHoverCard(nullptr);
  }

  ScopedObserver<views::View, views::ViewObserver> tab_close_button_observer_{
      this};

  Tab* tab_;
  views::View* close_button_;
  TabController* controller_;

  DISALLOW_COPY_AND_ASSIGN(TabCloseButtonObserver);
};

// Tab -------------------------------------------------------------------------

// static
const char Tab::kViewClassName[] = "Tab";

// static
void Tab::SetShowHoverCardOnMouseHoverForTesting(bool value) {
  g_show_hover_card_on_mouse_hover = value;
}

Tab::Tab(TabController* controller)
    : controller_(controller),
      title_(new views::Label()),
      title_animation_(this) {
  DCHECK(controller);

  tab_style_ = TabStyleViews::CreateForTab(this);

  // So we get don't get enter/exit on children and don't prematurely stop the
  // hover.
  set_notify_enter_exit_on_child(true);

  SetID(VIEW_ID_TAB);

  // This will cause calls to GetContentsBounds to return only the rectangle
  // inside the tab shape, rather than to its extents.
  SetBorder(views::CreateEmptyBorder(tab_style()->GetContentsInsets()));

  title_->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
  title_->SetElideBehavior(gfx::FADE_TAIL);
  title_->SetHandlesTooltips(false);
  title_->SetAutoColorReadabilityEnabled(false);
  title_->SetText(CoreTabHelper::GetDefaultTitle());
  title_->SetFontList(tab_style_->GetFontList());
  AddChildView(title_);

  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));

  icon_ = new TabIcon;
  AddChildView(icon_);

  alert_indicator_ = new AlertIndicator(this);
  AddChildView(alert_indicator_);

  // Unretained is safe here because this class outlives its close button, and
  // the controller outlives this Tab.
  close_button_ = new TabCloseButton(
      this, base::BindRepeating(&TabController::OnMouseEventInTab,
                                base::Unretained(controller_)));
  close_button_->set_has_ink_drop_action_on_click(true);
  AddChildView(close_button_);

  tab_close_button_observer_ = std::make_unique<TabCloseButtonObserver>(
      this, close_button_, controller_);

  title_animation_.SetDuration(base::TimeDelta::FromMilliseconds(100));

  // Enable keyboard focus.
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  focus_ring_ = views::FocusRing::Install(this);
  views::HighlightPathGenerator::Install(
      this, std::make_unique<TabStyleHighlightPathGenerator>(tab_style_.get()));
}

Tab::~Tab() {
  // Observer must be unregistered before child views are destroyed.
  tab_close_button_observer_.reset();
  if (controller_->HoverCardIsShowingForTab(this))
    controller_->UpdateHoverCard(nullptr);
}

void Tab::AnimationEnded(const gfx::Animation* animation) {
  DCHECK_EQ(animation, &title_animation_);
  title_->SetBoundsRect(target_title_bounds_);
}

void Tab::AnimationProgressed(const gfx::Animation* animation) {
  DCHECK_EQ(animation, &title_animation_);
  title_->SetBoundsRect(gfx::Tween::RectValueBetween(
      gfx::Tween::CalculateValue(gfx::Tween::FAST_OUT_SLOW_IN,
                                 animation->GetCurrentValue()),
      start_title_bounds_, target_title_bounds_));
}

void Tab::ButtonPressed(views::Button* sender, const ui::Event& event) {
  if (!alert_indicator_ || !alert_indicator_->GetVisible())
    base::RecordAction(UserMetricsAction("CloseTab_NoAlertIndicator"));
  else if (GetAlertStateToShow(data_.alert_state) ==
           TabAlertState::AUDIO_PLAYING)
    base::RecordAction(UserMetricsAction("CloseTab_AudioIndicator"));
  else
    base::RecordAction(UserMetricsAction("CloseTab_RecordingIndicator"));

  const CloseTabSource source = (event.type() == ui::ET_MOUSE_RELEASED &&
                                 !(event.flags() & ui::EF_FROM_TOUCH))
                                    ? CLOSE_TAB_FROM_MOUSE
                                    : CLOSE_TAB_FROM_TOUCH;
  DCHECK_EQ(close_button_, sender);
  controller_->CloseTab(this, source);
  if (event.type() == ui::ET_GESTURE_TAP)
    TouchUMA::RecordGestureAction(TouchUMA::kGestureTabCloseTap);
}

bool Tab::GetHitTestMask(SkPath* mask) const {
  // When the window is maximized we don't want to shave off the edges or top
  // shadow of the tab, such that the user can click anywhere along the top
  // edge of the screen to select a tab. Ditto for immersive fullscreen.
  *mask = tab_style()->GetPath(
      TabStyle::PathType::kHitTest,
      GetWidget()->GetCompositor()->device_scale_factor(),
      /* force_active */ false, TabStyle::RenderUnits::kDips);
  return true;
}

void Tab::Layout() {
  const gfx::Rect contents_rect = GetContentsBounds();

  const bool was_showing_icon = showing_icon_;
  UpdateIconVisibility();

  int start = contents_rect.x();
  if (extra_padding_before_content_) {
    constexpr int kExtraLeftPaddingToBalanceCloseButtonPadding = 4;
    start += kExtraLeftPaddingToBalanceCloseButtonPadding;
  }

  // The bounds for the favicon will include extra width for the attention
  // indicator, but visually it will be smaller at kFaviconSize wide.
  gfx::Rect favicon_bounds(start, contents_rect.y(), 0, 0);
  if (showing_icon_) {
    // Height should go to the bottom of the tab for the crashed tab animation
    // to pop out of the bottom.
    favicon_bounds.set_y(contents_rect.y() +
                         Center(contents_rect.height(), gfx::kFaviconSize));
    if (center_icon_) {
      // When centering the favicon, the favicon is allowed to escape the normal
      // contents rect.
      favicon_bounds.set_x(Center(width(), gfx::kFaviconSize));
    } else {
      MaybeAdjustLeftForPinnedTab(&favicon_bounds, gfx::kFaviconSize);
    }
    // Add space for insets outside the favicon bounds.
    favicon_bounds.Inset(-icon_->GetInsets());
    favicon_bounds.set_size(
        gfx::Size(icon_->GetPreferredSize().width(),
                  contents_rect.height() - favicon_bounds.y()));
  }
  icon_->SetBoundsRect(favicon_bounds);
  icon_->SetVisible(showing_icon_);

  const int after_title_padding = GetLayoutConstant(TAB_AFTER_TITLE_PADDING);

  int close_x = contents_rect.right();
  if (showing_close_button_) {
    // If the ratio of the close button size to tab width exceeds the maximum.
    // The close button should be as large as possible so that there is a larger
    // hit-target for touch events. So the close button bounds extends to the
    // edges of the tab. However, the larger hit-target should be active only
    // for touch events, and the close-image should show up in the right place.
    // So a border is added to the button with necessary padding. The close
    // button (Tab::TabCloseButton) makes sure the padding is a hit-target only
    // for touch events.
    // TODO(pkasting): The padding should maybe be removed, see comments in
    // TabCloseButton::TargetForRect().
    close_button_->SetBorder(views::NullBorder());
    const gfx::Size close_button_size(close_button_->GetPreferredSize());
    const int top = contents_rect.y() +
                    Center(contents_rect.height(), close_button_size.height());
    // Clamp the close button position to "centered within the tab"; this should
    // only have an effect when animating in a new active tab, which might start
    // out narrower than the minimum active tab width.
    close_x = std::max(contents_rect.right() - close_button_size.width(),
                       Center(width(), close_button_size.width()));
    const int left = std::min(after_title_padding, close_x);
    const int bottom = height() - close_button_size.height() - top;
    const int right =
        std::max(0, width() - (close_x + close_button_size.width()));
    close_button_->SetBorder(
        views::CreateEmptyBorder(top, left, bottom, right));
    close_button_->SetBoundsRect(
        {gfx::Point(close_x - left, 0), close_button_->GetPreferredSize()});
  }
  close_button_->SetVisible(showing_close_button_);

  if (showing_alert_indicator_) {
    int right = contents_rect.right();
    if (showing_close_button_) {
      right = close_x;
      if (extra_alert_indicator_padding_)
        right -= ui::TouchUiController::Get()->touch_ui() ? 8 : 6;
    }
    const gfx::Size image_size = alert_indicator_->GetPreferredSize();
    gfx::Rect bounds(
        std::max(contents_rect.x(), right - image_size.width()),
        contents_rect.y() + Center(contents_rect.height(), image_size.height()),
        image_size.width(), image_size.height());
    if (center_icon_) {
      // When centering the alert icon, it is allowed to escape the normal
      // contents rect.
      bounds.set_x(Center(width(), bounds.width()));
    } else {
      MaybeAdjustLeftForPinnedTab(&bounds, bounds.width());
    }
    alert_indicator_->SetBoundsRect(bounds);
  }
  alert_indicator_->SetVisible(showing_alert_indicator_);

  // Size the title to fill the remaining width and use all available height.
  bool show_title = ShouldRenderAsNormalTab();
  if (show_title) {
    int title_left = start;
    if (showing_icon_) {
      // When computing the spacing from the favicon, don't count the actual
      // icon view width (which will include extra room for the alert
      // indicator), but rather the normal favicon width which is what it will
      // look like.
      const int after_favicon = favicon_bounds.x() + icon_->GetInsets().left() +
                                gfx::kFaviconSize +
                                GetLayoutConstant(TAB_PRE_TITLE_PADDING);
      title_left = std::max(title_left, after_favicon);
    }
    int title_right = contents_rect.right();
    if (showing_alert_indicator_) {
      title_right = alert_indicator_->x() - after_title_padding;
    } else if (showing_close_button_) {
      // Allow the title to overlay the close button's empty border padding.
      title_right = close_x - after_title_padding;
    }
    const int title_width = std::max(title_right - title_left, 0);
    // The Label will automatically center the font's cap height within the
    // provided vertical space.
    const gfx::Rect title_bounds(title_left, contents_rect.y(), title_width,
                                 contents_rect.height());
    show_title = title_width > 0;

    if (title_bounds != target_title_bounds_) {
      target_title_bounds_ = title_bounds;
      if (was_showing_icon == showing_icon_ || title_->bounds().IsEmpty() ||
          title_bounds.IsEmpty()) {
        title_animation_.Stop();
        title_->SetBoundsRect(title_bounds);
      } else if (!title_animation_.is_animating()) {
        start_title_bounds_ = title_->bounds();
        title_animation_.Start();
      }
    }
  }
  title_->SetVisible(show_title);

  if (focus_ring_)
    focus_ring_->Layout();
}

const char* Tab::GetClassName() const {
  return kViewClassName;
}

bool Tab::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_RETURN && !IsSelected()) {
    controller_->SelectTab(this, event);
    return true;
  }

  constexpr int kModifiedFlag =
#if defined(OS_MACOSX)
      ui::EF_COMMAND_DOWN;
#else
      ui::EF_CONTROL_DOWN;
#endif

  if (event.type() == ui::ET_KEY_PRESSED && (event.flags() & kModifiedFlag)) {
    if (event.flags() & ui::EF_SHIFT_DOWN) {
      if (event.key_code() == ui::VKEY_RIGHT) {
        controller()->MoveTabLast(this);
        return true;
      }
      if (event.key_code() == ui::VKEY_LEFT) {
        controller()->MoveTabFirst(this);
        return true;
      }
    } else {
      if (event.key_code() == ui::VKEY_RIGHT) {
        controller()->ShiftTabRight(this);
        return true;
      }
      if (event.key_code() == ui::VKEY_LEFT) {
        controller()->ShiftTabLeft(this);
        return true;
      }
    }
  }

  return false;
}

bool Tab::OnKeyReleased(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_SPACE && !IsSelected()) {
    controller_->SelectTab(this, event);
    return true;
  }
  return false;
}

namespace {
bool IsSelectionModifierDown(const ui::MouseEvent& event) {
#if defined(OS_MACOSX)
  return event.IsCommandDown();
#else
  return event.IsControlDown();
#endif
}
}  // namespace

bool Tab::OnMousePressed(const ui::MouseEvent& event) {
  controller_->UpdateHoverCard(nullptr);
  controller_->OnMouseEventInTab(this, event);

  // Allow a right click from touch to drag, which corresponds to a long click.
  if (event.IsOnlyLeftMouseButton() ||
      (event.IsOnlyRightMouseButton() && event.flags() & ui::EF_FROM_TOUCH)) {
    ui::ListSelectionModel original_selection;
    original_selection = controller_->GetSelectionModel();
    // Changing the selection may cause our bounds to change. If that happens
    // the location of the event may no longer be valid. Create a copy of the
    // event in the parents coordinate, which won't change, and recreate an
    // event after changing so the coordinates are correct.
    ui::MouseEvent event_in_parent(event, static_cast<View*>(this), parent());
    if (controller_->SupportsMultipleSelection()) {
      if (event.IsShiftDown() && IsSelectionModifierDown(event)) {
        controller_->AddSelectionFromAnchorTo(this);
      } else if (event.IsShiftDown()) {
        controller_->ExtendSelectionTo(this);
      } else if (IsSelectionModifierDown(event)) {
        controller_->ToggleSelected(this);
        if (!IsSelected()) {
          // Don't allow dragging non-selected tabs.
          return false;
        }
      } else if (!IsSelected()) {
        controller_->SelectTab(this, event);
        base::RecordAction(UserMetricsAction("SwitchTab_Click"));
      }
    } else if (!IsSelected()) {
      controller_->SelectTab(this, event);
      base::RecordAction(UserMetricsAction("SwitchTab_Click"));
    }
    ui::MouseEvent cloned_event(event_in_parent, parent(),
                                static_cast<View*>(this));

    if (!closing())
      controller_->MaybeStartDrag(this, cloned_event, original_selection);
  }
  return true;
}

bool Tab::OnMouseDragged(const ui::MouseEvent& event) {
  controller_->ContinueDrag(this, event);
  return true;
}

void Tab::OnMouseReleased(const ui::MouseEvent& event) {
  controller_->OnMouseEventInTab(this, event);

  // Notify the drag helper that we're done with any potential drag operations.
  // Clean up the drag helper, which is re-created on the next mouse press.
  // In some cases, ending the drag will schedule the tab for destruction; if
  // so, bail immediately, since our members are already dead and we shouldn't
  // do anything else except drop the tab where it is.
  if (controller_->EndDrag(END_DRAG_COMPLETE))
    return;

  // Close tab on middle click, but only if the button is released over the tab
  // (normal windows behavior is to discard presses of a UI element where the
  // releases happen off the element).
  if (event.IsOnlyMiddleMouseButton()) {
    if (HitTestPoint(event.location())) {
      controller_->CloseTab(this, CLOSE_TAB_FROM_MOUSE);
    } else if (closing_) {
      // We're animating closed and a middle mouse button was pushed on us but
      // we don't contain the mouse anymore. We assume the user is clicking
      // quicker than the animation and we should close the tab that falls under
      // the mouse.
      gfx::Point location_in_parent = event.location();
      ConvertPointToTarget(this, parent(), &location_in_parent);
      Tab* closest_tab = controller_->GetTabAt(location_in_parent);
      if (closest_tab)
        controller_->CloseTab(closest_tab, CLOSE_TAB_FROM_MOUSE);
    }
  } else if (event.IsOnlyLeftMouseButton() && !event.IsShiftDown() &&
             !IsSelectionModifierDown(event)) {
    // If the tab was already selected mouse pressed doesn't change the
    // selection. Reset it now to handle the case where multiple tabs were
    // selected.
    controller_->SelectTab(this, event);
  }
}

void Tab::OnMouseCaptureLost() {
  controller_->EndDrag(END_DRAG_CAPTURE_LOST);
}

void Tab::OnMouseMoved(const ui::MouseEvent& event) {
  tab_style_->SetHoverLocation(event.location());
  controller_->OnMouseEventInTab(this, event);

  // Linux enter/leave events are sometimes flaky, so we don't want to "miss"
  // an enter event and fail to hover the tab.
  //
  // In Windows, we won't miss the enter event but mouse input is disabled after
  // a touch gesture and we could end up ignoring the enter event. If the user
  // subsequently moves the mouse, we need to then hover the tab.
  //
  // Either way, this is effectively a no-op if the tab is already in a hovered
  // state.
  MaybeUpdateHoverStatus(event);
}

void Tab::OnMouseEntered(const ui::MouseEvent& event) {
  MaybeUpdateHoverStatus(event);
}

void Tab::MaybeUpdateHoverStatus(const ui::MouseEvent& event) {
  if (mouse_hovered_ || !GetWidget()->IsMouseEventsEnabled())
    return;

#if defined(OS_LINUX)
  // Move the hit test area for hovering up so that it is not overlapped by tab
  // hover cards when they are shown.
  // TODO(crbug/978134): Once Linux/CrOS widget transparency is solved, remove
  // this case.
  constexpr int kHoverCardOverlap = 6;
  if (event.location().y() >= height() - kHoverCardOverlap)
    return;
#endif

  mouse_hovered_ = true;
  tab_style_->ShowHover(TabStyle::ShowHoverStyle::kSubtle);
  UpdateForegroundColors();
  Layout();
  if (g_show_hover_card_on_mouse_hover)
    controller_->UpdateHoverCard(this);
}

void Tab::OnMouseExited(const ui::MouseEvent& event) {
  if (!mouse_hovered_)
    return;
  mouse_hovered_ = false;
  tab_style_->HideHover(TabStyle::HideHoverStyle::kGradual);
  UpdateForegroundColors();
  Layout();
}

void Tab::OnGestureEvent(ui::GestureEvent* event) {
  controller_->UpdateHoverCard(nullptr);
  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN: {
      // TAP_DOWN is only dispatched for the first touch point.
      DCHECK_EQ(1, event->details().touch_points());

      // See comment in OnMousePressed() as to why we copy the event.
      ui::GestureEvent event_in_parent(*event, static_cast<View*>(this),
                                       parent());
      ui::ListSelectionModel original_selection;
      original_selection = controller_->GetSelectionModel();
      tab_activated_with_last_tap_down_ = !IsActive();
      if (!IsSelected())
        controller_->SelectTab(this, *event);
      gfx::Point loc(event->location());
      views::View::ConvertPointToScreen(this, &loc);
      ui::GestureEvent cloned_event(event_in_parent, parent(),
                                    static_cast<View*>(this));

      if (!closing())
        controller_->MaybeStartDrag(this, cloned_event, original_selection);
      break;
    }

    default:
      break;
  }
  event->SetHandled();
}

base::string16 Tab::GetTooltipText(const gfx::Point& p) const {
  // TODO(corising): Make sure that accessibility is solved properly for hover
  // cards.
  // Tab hover cards replace tooltips.
  if (base::FeatureList::IsEnabled(features::kTabHoverCards))
    return base::string16();

  // Note: Anything that affects the tooltip text should be accounted for when
  // calling TooltipTextChanged() from Tab::SetData().
  return GetTooltipText(data_.title, GetAlertStateToShow(data_.alert_state));
}

void Tab::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kTab;
  node_data->AddState(ax::mojom::State::kMultiselectable);
  node_data->AddBoolAttribute(ax::mojom::BoolAttribute::kSelected,
                              IsSelected());

  base::string16 name = controller_->GetAccessibleTabName(this);
  if (!name.empty()) {
    node_data->SetName(name);
  } else {
    // Under some conditions, |GetAccessibleTabName| returns an empty string.
    node_data->SetNameExplicitlyEmpty();
  }
}

gfx::Size Tab::CalculatePreferredSize() const {
  return gfx::Size(TabStyle::GetStandardWidth(), GetLayoutConstant(TAB_HEIGHT));
}

void Tab::PaintChildren(const views::PaintInfo& info) {
  // Clip children based on the tab's fill path.  This has no effect except when
  // the tab is too narrow to completely show even one icon, at which point this
  // serves to clip the favicon.
  ui::ClipRecorder clip_recorder(info.context());
  // The paint recording scale for tabs is consistent along the x and y axis.
  const float paint_recording_scale = info.paint_recording_scale_x();

  const SkPath clip_path = tab_style()->GetPath(
      TabStyle::PathType::kInteriorClip, paint_recording_scale);

  clip_recorder.ClipPathWithAntiAliasing(clip_path);
  View::PaintChildren(info);
}

void Tab::OnPaint(gfx::Canvas* canvas) {
  tab_style()->PaintTab(canvas);
}

void Tab::AddedToWidget() {
  UpdateForegroundColors();
}

void Tab::OnFocus() {
  View::OnFocus();
  controller_->UpdateHoverCard(this);
}

void Tab::OnBlur() {
  View::OnBlur();
  controller_->UpdateHoverCard(nullptr);
}

void Tab::OnThemeChanged() {
  TabSlotView::OnThemeChanged();
  UpdateForegroundColors();
}

TabSlotView::ViewType Tab::GetTabSlotViewType() const {
  return TabSlotView::ViewType::kTab;
}

TabSizeInfo Tab::GetTabSizeInfo() const {
  return {TabStyle::GetPinnedWidth(), TabStyleViews::GetMinimumActiveWidth(),
          TabStyleViews::GetMinimumInactiveWidth(),
          TabStyle::GetStandardWidth()};
}

void Tab::SetClosing(bool closing) {
  closing_ = closing;
  ActiveStateChanged();

  if (closing) {
    // When closing, sometimes DCHECK fails because
    // cc::Layer::IsPropertyChangeAllowed() returns false. Deleting
    // the focus ring fixes this. TODO(collinbaker): investigate why
    // this happens.
    focus_ring_.reset();
  }
}

base::Optional<SkColor> Tab::GetGroupColor() const {
  if (closing_ || !group().has_value())
    return base::nullopt;

  return controller_->GetPaintedGroupColor(
      controller_->GetGroupColorId(group().value()));
}

SkColor Tab::GetAlertIndicatorColor(TabAlertState state) const {
  // If theme provider is not yet available, return the default button
  // color.
  const ui::ThemeProvider* theme_provider = GetThemeProvider();
  if (!theme_provider)
    return foreground_color_;

  switch (state) {
    case TabAlertState::AUDIO_PLAYING:
    case TabAlertState::AUDIO_MUTING:
      return theme_provider->GetColor(ThemeProperties::COLOR_TAB_ALERT_AUDIO);
    case TabAlertState::MEDIA_RECORDING:
    case TabAlertState::DESKTOP_CAPTURING:
      return theme_provider->GetColor(
          ThemeProperties::COLOR_TAB_ALERT_RECORDING);
    case TabAlertState::TAB_CAPTURING:
      return theme_provider->GetColor(
          ThemeProperties::COLOR_TAB_ALERT_CAPTURING);
    case TabAlertState::PIP_PLAYING:
      return theme_provider->GetColor(ThemeProperties::COLOR_TAB_PIP_PLAYING);
    case TabAlertState::BLUETOOTH_CONNECTED:
    case TabAlertState::USB_CONNECTED:
    case TabAlertState::HID_CONNECTED:
    case TabAlertState::SERIAL_CONNECTED:
    case TabAlertState::VR_PRESENTING_IN_HEADSET:
      return foreground_color_;
    default:
      NOTREACHED();
      return foreground_color_;
  }
}

bool Tab::IsActive() const {
  return controller_->IsActiveTab(this);
}

void Tab::ActiveStateChanged() {
  UpdateTabIconNeedsAttentionBlocked();
  UpdateForegroundColors();
  title_->SetFontList(tab_style_->GetFontList());
  Layout();
}

void Tab::AlertStateChanged() {
  if (controller_->HoverCardIsShowingForTab(this))
    controller_->UpdateHoverCard(this);
  Layout();
}

void Tab::FrameColorsChanged() {
  UpdateForegroundColors();
}

void Tab::SelectedStateChanged() {
  UpdateForegroundColors();
}

bool Tab::IsSelected() const {
  return controller_->IsTabSelected(this);
}

void Tab::SetData(TabRendererData data) {
  DCHECK(GetWidget());

  if (data_ == data)
    return;

  TabRendererData old(std::move(data_));
  data_ = std::move(data);

  icon_->SetData(data_);
  icon_->SetCanPaintToLayer(controller_->CanPaintThrobberToLayer());
  UpdateTabIconNeedsAttentionBlocked();

  base::string16 title = data_.title;
  if (title.empty() && !data_.should_render_empty_title) {
    title = icon_->ShowingLoadingAnimation()
                ? l10n_util::GetStringUTF16(IDS_TAB_LOADING_TITLE)
                : CoreTabHelper::GetDefaultTitle();
  } else {
    title = Browser::FormatTitleForDisplay(title);
  }
  title_->SetText(title);

  const auto new_alert_state = GetAlertStateToShow(data_.alert_state);
  const auto old_alert_state = GetAlertStateToShow(old.alert_state);
  if (new_alert_state != old_alert_state)
    alert_indicator_->TransitionToAlertState(new_alert_state);
  if (old.pinned != data_.pinned)
    showing_alert_indicator_ = false;

  if (new_alert_state != old_alert_state || data_.title != old.title)
    TooltipTextChanged();

  Layout();
  SchedulePaint();
}

void Tab::StepLoadingAnimation(const base::TimeDelta& elapsed_time) {
  icon_->StepLoadingAnimation(elapsed_time);

  // Update the layering if necessary.
  //
  // TODO(brettw) this design should be changed to be a push state when the tab
  // can't be painted to a layer, rather than continually polling the
  // controller about the state and reevaulating that state in the icon. This
  // is both overly aggressive and wasteful in the common case, and not
  // frequent enough in other cases since the state can be updated and the tab
  // painted before the animation is stepped.
  icon_->SetCanPaintToLayer(controller_->CanPaintThrobberToLayer());
}

void Tab::SetTabNeedsAttention(bool attention) {
  icon_->SetAttention(TabIcon::AttentionType::kTabWantsAttentionStatus,
                      attention);
  SchedulePaint();
}

// static
base::string16 Tab::GetTooltipText(const base::string16& title,
                                   base::Optional<TabAlertState> alert_state) {
  if (!alert_state)
    return title;

  base::string16 result = title;
  if (!result.empty())
    result.append(1, '\n');
  result.append(chrome::GetTabAlertStateText(alert_state.value()));
  return result;
}

// static
base::Optional<TabAlertState> Tab::GetAlertStateToShow(
    const std::vector<TabAlertState>& alert_states) {
  if (alert_states.empty())
    return base::nullopt;

  return alert_states[0];
}

void Tab::MaybeAdjustLeftForPinnedTab(gfx::Rect* bounds,
                                      int visual_width) const {
  if (ShouldRenderAsNormalTab())
    return;
  const int pinned_width = TabStyle::GetPinnedWidth();
  const int ideal_delta = width() - pinned_width;
  const int ideal_x = (pinned_width - visual_width) / 2;
  // TODO(pkasting): https://crbug.com/533570  This code is broken when the
  // current width is less than the pinned width.
  bounds->set_x(
      bounds->x() +
      gfx::ToRoundedInt(
          (1 - static_cast<float>(ideal_delta) /
                   static_cast<float>(kPinnedTabExtraWidthToRenderAsNormal)) *
          (ideal_x - bounds->x())));
}

void Tab::UpdateIconVisibility() {
  // TODO(pkasting): This whole function should go away, and we should simply
  // compute child visibility state in Layout().

  // Don't adjust whether we're centering the favicon or adding extra padding
  // during tab closure; let it stay however it was prior to closing the tab.
  // This prevents the icon and text from sliding left at the end of closing
  // a non-narrow tab.
  if (!closing_) {
    center_icon_ = false;
    extra_padding_before_content_ = false;
  }

  showing_icon_ = showing_alert_indicator_ = false;
  extra_alert_indicator_padding_ = false;

  if (height() < GetLayoutConstant(TAB_HEIGHT))
    return;

  const bool has_favicon = data().show_icon;
  const bool has_alert_icon =
      (alert_indicator_ ? alert_indicator_->showing_alert_state()
                        : GetAlertStateToShow(data().alert_state))
          .has_value();

  if (data().pinned) {
    // When the tab is pinned, we can show one of the two icons; the alert icon
    // is given priority over the favicon. The close buton is never shown.
    showing_alert_indicator_ = has_alert_icon;
    showing_icon_ = has_favicon && !has_alert_icon;
    showing_close_button_ = false;
    return;
  }

  int available_width = GetContentsBounds().width();

  const bool touch_ui = ui::TouchUiController::Get()->touch_ui();
  const int favicon_width = gfx::kFaviconSize;
  const int alert_icon_width = alert_indicator_->GetPreferredSize().width();
  // In case of touch optimized UI, the close button has an extra padding on the
  // left that needs to be considered.
  const int close_button_width =
      close_button_->GetPreferredSize().width() -
      (touch_ui ? close_button_->GetInsets().right()
                : close_button_->GetInsets().width());
  const bool large_enough_for_close_button =
      available_width >= (touch_ui ? kTouchMinimumContentsWidthForCloseButtons
                                   : kMinimumContentsWidthForCloseButtons);

  showing_close_button_ = !controller_->ShouldHideCloseButtonForTab(this);
  if (IsActive()) {
    // Close button is shown on active tabs regardless of the size.
    if (showing_close_button_)
      available_width -= close_button_width;

    showing_alert_indicator_ =
        has_alert_icon && alert_icon_width <= available_width;
    if (showing_alert_indicator_)
      available_width -= alert_icon_width;

    showing_icon_ = has_favicon && favicon_width <= available_width;
    if (showing_icon_)
      available_width -= favicon_width;
  } else {
    showing_alert_indicator_ =
        has_alert_icon && alert_icon_width <= available_width;
    if (showing_alert_indicator_)
      available_width -= alert_icon_width;

    showing_icon_ = has_favicon && favicon_width <= available_width;
    if (showing_icon_)
      available_width -= favicon_width;

    showing_close_button_ &= large_enough_for_close_button;
    if (showing_close_button_)
      available_width -= close_button_width;

    // If no other controls are visible, show the alert icon or the favicon
    // even though we don't have enough space. We'll clip the icon in
    // PaintChildren().
    if (!showing_close_button_ && !showing_alert_indicator_ && !showing_icon_) {
      showing_alert_indicator_ = has_alert_icon;
      showing_icon_ = !showing_alert_indicator_ && has_favicon;

      // See comments near top of function on why this conditional is here.
      if (!closing_)
        center_icon_ = true;
    }
  }

  // Don't update padding while the tab is closing, to avoid glitchy-looking
  // behaviour when the close animation causes the tab to get very small
  if (!closing_) {
    // The extra padding is intended to visually balance the close button, so
    // only include it when the close button is shown or will be shown on hover.
    // We also check this for active tabs so that the extra padding doesn't pop
    // in and out as you switch tabs.
    extra_padding_before_content_ = large_enough_for_close_button;
  }

  extra_alert_indicator_padding_ = showing_alert_indicator_ &&
                                   showing_close_button_ &&
                                   large_enough_for_close_button;
}

bool Tab::ShouldRenderAsNormalTab() const {
  return !data().pinned || (width() >= (TabStyle::GetPinnedWidth() +
                                        kPinnedTabExtraWidthToRenderAsNormal));
}

void Tab::UpdateTabIconNeedsAttentionBlocked() {
  // Only show the blocked attention indicator on non-active tabs. For active
  // tabs, the user sees the dialog blocking the tab, so there's no point to it
  // and it would be distracting.
  if (IsActive()) {
    icon_->SetAttention(TabIcon::AttentionType::kBlockedWebContents, false);
  } else {
    icon_->SetAttention(TabIcon::AttentionType::kBlockedWebContents,
                        data_.blocked);
  }
}

void Tab::UpdateForegroundColors() {
  TabStyle::TabColors colors = tab_style_->CalculateColors();

  title_->SetEnabledColor(colors.foreground_color);

  close_button_->SetIconColors(colors.foreground_color,
                               colors.background_color);

  if (foreground_color_ != colors.foreground_color) {
    foreground_color_ = colors.foreground_color;
    alert_indicator_->OnParentTabButtonColorChanged();
  }

  SchedulePaint();
}
