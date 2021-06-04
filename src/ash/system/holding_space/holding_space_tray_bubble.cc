// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_tray_bubble.h"

#include <vector>

#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_metrics.h"
#include "ash/public/cpp/holding_space/holding_space_prefs.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/holding_space/holding_space_item_view.h"
#include "ash/system/holding_space/holding_space_tray.h"
#include "ash/system/holding_space/pinned_files_bubble.h"
#include "ash/system/holding_space/recent_files_bubble.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/wm/work_area_insets.h"
#include "base/containers/adapters.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/proposed_layout.h"

namespace ash {

namespace {

// Animation.
constexpr base::TimeDelta kAnimationDuration =
    base::TimeDelta::FromMilliseconds(167);

// Helpers ---------------------------------------------------------------------

// Finds all visible `HoldingSpaceItem`s in `parent`'s view hierarchy.
void FindVisibleHoldingSpaceItems(
    views::View* parent,
    std::vector<const HoldingSpaceItem*>* result) {
  for (views::View* view : parent->children()) {
    if (view->GetVisible() && HoldingSpaceItemView::IsInstance(view))
      result->push_back(HoldingSpaceItemView::Cast(view)->item());
    FindVisibleHoldingSpaceItems(view, result);
  }
}

// Records the time from first availability to first entry into holding space.
void RecordTimeFromFirstAvailabilityToFirstEntry(PrefService* prefs) {
  base::Time time_of_first_availability =
      holding_space_prefs::GetTimeOfFirstAvailability(prefs).value();
  base::Time time_of_first_entry =
      holding_space_prefs::GetTimeOfFirstEntry(prefs).value();
  holding_space_metrics::RecordTimeFromFirstAvailabilityToFirstEntry(
      time_of_first_entry - time_of_first_availability);
}

// HoldingSpaceTrayBubbleEventHandler ------------------------------------------

class HoldingSpaceTrayBubbleEventHandler : public ui::EventHandler {
 public:
  HoldingSpaceTrayBubbleEventHandler(HoldingSpaceTrayBubble* bubble,
                                     HoldingSpaceItemViewDelegate* delegate)
      : bubble_(bubble), delegate_(delegate) {
    aura::Env::GetInstance()->AddPreTargetHandler(
        this, ui::EventTarget::Priority::kSystem);
  }

  HoldingSpaceTrayBubbleEventHandler(
      const HoldingSpaceTrayBubbleEventHandler&) = delete;
  HoldingSpaceTrayBubbleEventHandler& operator=(
      const HoldingSpaceTrayBubbleEventHandler&) = delete;

  ~HoldingSpaceTrayBubbleEventHandler() override {
    aura::Env::GetInstance()->RemovePreTargetHandler(this);
  }

 private:
  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override {
    if (event->type() != ui::ET_KEY_PRESSED)
      return;

    // Only handle `event`s that would otherwise escape the `bubble_` window.
    aura::Window* target = static_cast<aura::Window*>(event->target());
    aura::Window* bubble_window = bubble_->GetBubbleWidget()->GetNativeView();
    if (target && (bubble_window->Contains(target)))
      return;

    // If `delegate_` handles the `event`, prevent additional bubbling up.
    if (delegate_->OnHoldingSpaceTrayBubbleKeyPressed(*event))
      event->StopPropagation();
  }

  HoldingSpaceTrayBubble* const bubble_;
  HoldingSpaceItemViewDelegate* const delegate_;
};

// ChildBubbleContainerLayout --------------------------------------------------

// A class similar to a `views::LayoutManager` which supports calculating and
// applying `views::ProposedLayout`s. Views are laid out similar to a vertical
// `views::BoxLayout` with the first child flexing to cede layout space if the
// layout would otherwise exceed maximum height restrictions. Subsequent child
// views will be laid outside of `host` bounds if there is insufficient space.
class ChildBubbleContainerLayout {
 public:
  ChildBubbleContainerLayout(views::View* host, int child_spacing)
      : host_(host), child_spacing_(child_spacing) {}

  // Sets the maximum height restriction for the layout.
  void SetMaxHeight(int max_height) { max_height_ = max_height; }

  // Calculates and returns a `views::ProposedLayout` given current maximum
  // height restrictions and the current state of the view hierarchy. Note that
  // views are laid out similar to a vertical `views::BoxLayout` with the first
  // child flexing to cede layout space if the layout would otherwise exceed
  // maximum height restrictions.
  views::ProposedLayout CalculateProposedLayout() const {
    views::ProposedLayout layout;
    layout.host_size = gfx::Size(kHoldingSpaceBubbleWidth, 0);

    int top = 0;
    for (views::View* child : host_->children()) {
      if (!child->GetVisible()) {
        views::ChildLayout child_layout;
        child_layout.child_view = child;
        child_layout.bounds = gfx::Rect(0, top, layout.host_size.width(), 0);
        child_layout.visible = false;
        layout.child_layouts.push_back(std::move(child_layout));
        continue;
      }

      // Apply child spacing.
      if (top != 0) {
        top += child_spacing_;
        layout.host_size.Enlarge(0, child_spacing_);
      }

      const int height = child->GetHeightForWidth(layout.host_size.width());

      views::ChildLayout child_layout;
      child_layout.child_view = child;
      child_layout.bounds = gfx::Rect(0, top, layout.host_size.width(), height);
      child_layout.visible = true;
      layout.child_layouts.push_back(std::move(child_layout));
      layout.host_size.Enlarge(0, height);

      top += height;
    }

    // If maximum height restrictions are present and preferred height exceeds
    // maximum height, the first child view should cede layout space for others.
    // Note that subsequent child views will still be given their preferred
    // height so its possible they will be laid outside of `host_` view bounds.
    if (max_height_ && layout.host_size.height() > max_height_) {
      const int height_to_cede =
          std::min(layout.child_layouts[0].bounds.height(),
                   layout.host_size.height() - max_height_);
      layout.child_layouts[0].bounds.Inset(0, 0, 0, height_to_cede);
      for (size_t i = 1; i < layout.child_layouts.size(); ++i)
        layout.child_layouts[i].bounds.Offset(0, -height_to_cede);
      layout.host_size.Enlarge(0, -height_to_cede);
    }

    return layout;
  }

  // Applies the specified `layout` to the view hierarchy.
  void ApplyLayout(const views::ProposedLayout& layout) {
    for (const auto& child_layout : layout.child_layouts)
      child_layout.child_view->SetBoundsRect(child_layout.bounds);
  }

  views::View* const host_;
  const int child_spacing_;

  // Maximum height restriction for the layout. If zero, it is assumed that
  // there is no maximum height restriction.
  int max_height_ = 0;
};

}  // namespace

// HoldingSpaceTrayBubble::ChildBubbleContainer --------------------------------

// The container for `HoldingSpaceTrayBubble` which parents its child bubbles
// and animates layout changes. Note that this view uses a pseudo layout manager
// to calculate bounds for its children, but animates any layout changes itself.
class HoldingSpaceTrayBubble::ChildBubbleContainer
    : public views::View,
      public views::AnimationDelegateViews {
 public:
  ChildBubbleContainer()
      : views::AnimationDelegateViews(this),
        layout_manager_(this, kHoldingSpaceBubbleContainerChildSpacing) {}

  // Sets the maximum height restriction for the layout.
  void SetMaxHeight(int max_height) {
    layout_manager_.SetMaxHeight(max_height);
    PreferredSizeChanged();
  }

  // views::View:
  int GetHeightForWidth(int width) const override {
    DCHECK_EQ(width, kHoldingSpaceBubbleWidth);
    if (current_layout_.host_size.IsEmpty())
      current_layout_ = layout_manager_.CalculateProposedLayout();
    return current_layout_.host_size.height();
  }

  void ChildPreferredSizeChanged(views::View* child) override {
    PreferredSizeChanged();
  }

  void ChildVisibilityChanged(views::View* child) override {
    PreferredSizeChanged();
  }

  void PreferredSizeChanged() override {
    if (!GetWidget())
      return;

    const views::ProposedLayout target_layout(
        layout_manager_.CalculateProposedLayout());

    // If `target_layout_` is unchanged then a layout animation is in progress
    // and the only thing needed is to propagate the event up the tree so that
    // the widget will be resized and re-anchored.
    if (target_layout == target_layout_) {
      views::View::PreferredSizeChanged();
      return;
    }

    // If `current_layout_` is empty then this is the first layout. Don't
    // animate the first layout.
    if (current_layout_.host_size.IsEmpty()) {
      current_layout_ = target_layout_ = target_layout;
      views::View::PreferredSizeChanged();
      return;
    }

    start_layout_ = current_layout_;
    target_layout_ = target_layout;

    // Animate changes from the `current_layout_` to the `target_layout_`. Note
    // the use of a throughput tracker to record layout animation smoothness.
    layout_animation_ = std::make_unique<gfx::SlideAnimation>(this);
    layout_animation_->SetSlideDuration(
        ui::ScopedAnimationDurationScaleMode::duration_multiplier() *
        kAnimationDuration);
    layout_animation_->SetTweenType(gfx::Tween::Type::FAST_OUT_SLOW_IN);
    layout_animation_throughput_tracker_ =
        GetWidget()->GetCompositor()->RequestNewThroughputTracker();
    layout_animation_throughput_tracker_->Start(
        metrics_util::ForSmoothness(base::BindRepeating(
            holding_space_metrics::RecordBubbleResizeAnimationSmoothness)));
    layout_animation_->Show();
  }

  void Layout() override { layout_manager_.ApplyLayout(current_layout_); }

  // views::AnimationDelegateViews:
  void AnimationProgressed(const gfx::Animation* animation) override {
    current_layout_ = views::ProposedLayoutBetween(
        animation->GetCurrentValue(), start_layout_, target_layout_);
    PreferredSizeChanged();
  }

  void AnimationEnded(const gfx::Animation* animation) override {
    current_layout_ = target_layout_;
    PreferredSizeChanged();

    // Record layout animation smoothness.
    layout_animation_throughput_tracker_->Stop();
    layout_animation_throughput_tracker_.reset();
  }

 private:
  // A pseudo layout manager which supports calculating and applying
  // `views::ProposedLayouts`. It lays out views similarly to a vertical
  // `views::BoxLayout` with the first view flexing to cede layout space to
  // siblings if maximum height restrictions would otherwise be exceeded.
  ChildBubbleContainerLayout layout_manager_;

  mutable views::ProposedLayout start_layout_;    // Layout being animated from.
  mutable views::ProposedLayout current_layout_;  // Current layout.
  mutable views::ProposedLayout target_layout_;   // Layout being animated to.

  std::unique_ptr<gfx::SlideAnimation> layout_animation_;
  absl::optional<ui::ThroughputTracker> layout_animation_throughput_tracker_;
};

// HoldingSpaceTrayBubble ------------------------------------------------------

HoldingSpaceTrayBubble::HoldingSpaceTrayBubble(
    HoldingSpaceTray* holding_space_tray)
    : holding_space_tray_(holding_space_tray) {
  TrayBubbleView::InitParams init_params;
  init_params.delegate = holding_space_tray;
  init_params.parent_window = holding_space_tray->GetBubbleWindowContainer();
  init_params.anchor_view = nullptr;
  init_params.anchor_mode = TrayBubbleView::AnchorMode::kRect;
  init_params.anchor_rect =
      holding_space_tray->shelf()->GetSystemTrayAnchorRect();
  init_params.bg_color = SK_ColorTRANSPARENT;
  init_params.insets = GetTrayBubbleInsets();
  init_params.shelf_alignment = holding_space_tray->shelf()->alignment();
  init_params.preferred_width = kHoldingSpaceBubbleWidth;
  init_params.close_on_deactivate = true;
  init_params.has_shadow = false;
  init_params.reroute_event_handler = true;

  // Create and customize bubble view.
  TrayBubbleView* bubble_view = new TrayBubbleView(init_params);
  // Ensure bubble frame does not draw background behind bubble view.
  bubble_view->set_color(SK_ColorTRANSPARENT);
  child_bubble_container_ =
      bubble_view->AddChildView(std::make_unique<ChildBubbleContainer>());
  child_bubble_container_->SetMaxHeight(CalculateMaxHeight());

  // Add pinned files child bubble.
  child_bubbles_.push_back(child_bubble_container_->AddChildView(
      std::make_unique<PinnedFilesBubble>(&delegate_)));

  // Add recent files child bubble.
  child_bubbles_.push_back(child_bubble_container_->AddChildView(
      std::make_unique<RecentFilesBubble>(&delegate_)));

  // Initialize child bubbles.
  for (HoldingSpaceTrayChildBubble* child_bubble : child_bubbles_)
    child_bubble->Init();

  // Show the bubble.
  bubble_wrapper_ =
      std::make_unique<TrayBubbleWrapper>(holding_space_tray, bubble_view);

  event_handler_ =
      std::make_unique<HoldingSpaceTrayBubbleEventHandler>(this, &delegate_);

  PrefService* const prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();

  // Mark when holding space was first entered. If this is not the first entry
  // into holding space, this will no-op. If this is the first entry, record the
  // amount of time from first availability to first entry into holding space.
  if (holding_space_prefs::MarkTimeOfFirstEntry(prefs))
    RecordTimeFromFirstAvailabilityToFirstEntry(prefs);

  // Record visible holding space items.
  std::vector<const HoldingSpaceItem*> visible_items;
  FindVisibleHoldingSpaceItems(bubble_view, &visible_items);
  holding_space_metrics::RecordItemCounts(visible_items);

  shelf_observation_.Observe(holding_space_tray_->shelf());
  tablet_mode_observation_.Observe(Shell::Get()->tablet_mode_controller());
}

HoldingSpaceTrayBubble::~HoldingSpaceTrayBubble() {
  bubble_wrapper_->bubble_view()->ResetDelegate();

  // Explicitly reset child bubbles so that they will stop observing the holding
  // space controller/model while they are asynchronously destroyed.
  for (HoldingSpaceTrayChildBubble* child_bubble : child_bubbles_)
    child_bubble->Reset();
}

void HoldingSpaceTrayBubble::AnchorUpdated() {
  bubble_wrapper_->bubble_view()->UpdateBubble();
}

TrayBubbleView* HoldingSpaceTrayBubble::GetBubbleView() {
  return bubble_wrapper_->bubble_view();
}

views::Widget* HoldingSpaceTrayBubble::GetBubbleWidget() {
  return bubble_wrapper_->GetBubbleWidget();
}

std::vector<HoldingSpaceItemView*>
HoldingSpaceTrayBubble::GetHoldingSpaceItemViews() {
  std::vector<HoldingSpaceItemView*> views;
  for (HoldingSpaceTrayChildBubble* child_bubble : child_bubbles_) {
    auto child_bubble_views = child_bubble->GetHoldingSpaceItemViews();
    views.insert(views.end(), child_bubble_views.begin(),
                 child_bubble_views.end());
  }
  return views;
}

int HoldingSpaceTrayBubble::CalculateMaxHeight() const {
  const WorkAreaInsets* work_area = WorkAreaInsets::ForWindow(
      holding_space_tray_->shelf()->GetWindow()->GetRootWindow());

  const int bottom =
      holding_space_tray_->shelf()->IsHorizontalAlignment()
          ? holding_space_tray_->shelf()->GetShelfBoundsInScreen().y()
          : work_area->user_work_area_bounds().bottom();

  const int free_space_height_above_anchor =
      bottom - work_area->user_work_area_bounds().y();

  const gfx::Insets insets = GetTrayBubbleInsets();
  const int bubble_vertical_margin = insets.top() + insets.bottom();

  return free_space_height_above_anchor - bubble_vertical_margin;
}

void HoldingSpaceTrayBubble::UpdateBubbleBounds() {
  child_bubble_container_->SetMaxHeight(CalculateMaxHeight());
  bubble_wrapper_->bubble_view()->ChangeAnchorRect(
      holding_space_tray_->shelf()->GetSystemTrayAnchorRect());
}

void HoldingSpaceTrayBubble::OnDisplayConfigurationChanged() {
  UpdateBubbleBounds();
}

void HoldingSpaceTrayBubble::OnAutoHideStateChanged(ShelfAutoHideState state) {
  UpdateBubbleBounds();
}

void HoldingSpaceTrayBubble::OnTabletModeStarted() {
  UpdateBubbleBounds();
}

void HoldingSpaceTrayBubble::OnTabletModeEnded() {
  UpdateBubbleBounds();
}

}  // namespace ash
