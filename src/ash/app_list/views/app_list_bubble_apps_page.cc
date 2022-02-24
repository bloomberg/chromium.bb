// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_bubble_apps_page.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/views/app_list_nudge_controller.h"
#include "ash/app_list/views/app_list_toast_container_view.h"
#include "ash/app_list/views/app_list_view_util.h"
#include "ash/app_list/views/continue_section_view.h"
#include "ash/app_list/views/recent_apps_view.h"
#include "ash/app_list/views/scrollable_apps_grid_view.h"
#include "ash/bubble/bubble_utils.h"
#include "ash/constants/ash_features.h"
#include "ash/controls/rounded_scroll_bar.h"
#include "ash/controls/scroll_view_gradient_helper.h"
#include "ash/public/cpp/metrics_util.h"
#include "ash/public/cpp/style/color_provider.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/aura/window.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/animation_throughput_reporter.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/layer_type.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

using views::BoxLayout;

namespace ash {

namespace {

constexpr int kContinueColumnCount = 2;

// Insets for the vertical scroll bar.
constexpr gfx::Insets kVerticalScrollInsets(1, 0, 1, 1);

// The padding between different sections within the apps page. Also used for
// interior apps page container margin.
constexpr int kVerticalPaddingBetweenSections = 16;

// The horizontal interior margin for the apps page container - i.e. the margin
// between the apps page bounds and the page content.
constexpr int kHorizontalInteriorMargin = 20;

// Insets for the separator between the continue section and apps.
constexpr gfx::Insets kSeparatorInsets(0, 12);

// A slide animation's tween type.
constexpr gfx::Tween::Type kSlideAnimationTweenType =
    gfx::Tween::LINEAR_OUT_SLOW_IN;

// Delay for the show page transform and opacity animations.
constexpr base::TimeDelta kShowPageAnimationDelay = base::Milliseconds(50);

// The spec says "Down 40 -> 0, duration 250ms" with no delay, but the opacity
// animation has a 50ms delay that causes the first 50ms to be invisible. Just
// animate the 200ms visible part, which is 32 dips. This ensures the search
// page hide animation doesn't play at the same time as the apps page show
// animation.
constexpr int kShowPageAnimationVerticalOffset = 32;
constexpr base::TimeDelta kShowPageAnimationTransformDuration =
    base::Milliseconds(200);

// Duration of the show page opacity animation.
constexpr base::TimeDelta kShowPageAnimationOpacityDuration =
    base::Milliseconds(100);

// The time duration of the undo toast fade in animation.
constexpr base::TimeDelta kToastFadeInAnimationDuration =
    base::Milliseconds(400);

}  // namespace

AppListBubbleAppsPage::AppListBubbleAppsPage(
    AppListViewDelegate* view_delegate,
    ApplicationDragAndDropHost* drag_and_drop_host,
    AppListConfig* app_list_config,
    AppListA11yAnnouncer* a11y_announcer,
    AppListFolderController* folder_controller)
    : app_list_nudge_controller_(std::make_unique<AppListNudgeController>()) {
  DCHECK(view_delegate);
  DCHECK(drag_and_drop_host);
  DCHECK(a11y_announcer);
  DCHECK(folder_controller);

  AppListModelProvider::Get()->AddObserver(this);

  SetUseDefaultFillLayout(true);

  // The entire page scrolls.
  scroll_view_ = AddChildView(std::make_unique<views::ScrollView>(
      views::ScrollView::ScrollWithLayers::kEnabled));
  scroll_view_->ClipHeightTo(0, std::numeric_limits<int>::max());
  scroll_view_->SetDrawOverflowIndicator(false);
  // Don't paint a background. The bubble already has one.
  scroll_view_->SetBackgroundColor(absl::nullopt);
  // Arrow keys are used to select app icons.
  scroll_view_->SetAllowKeyboardScrolling(false);

  // Scroll view will have a gradient mask layer.
  scroll_view_->SetPaintToLayer(ui::LAYER_NOT_DRAWN);

  // Set up scroll bars.
  scroll_view_->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  auto vertical_scroll =
      std::make_unique<RoundedScrollBar>(/*horizontal=*/false);
  vertical_scroll->SetInsets(kVerticalScrollInsets);
  scroll_view_->SetVerticalScrollBar(std::move(vertical_scroll));

  auto scroll_contents = std::make_unique<views::View>();
  auto* layout = scroll_contents->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kVertical,
      gfx::Insets(kVerticalPaddingBetweenSections, kHorizontalInteriorMargin),
      kVerticalPaddingBetweenSections));
  layout->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kStretch);

  // Continue section row.
  continue_section_ =
      scroll_contents->AddChildView(std::make_unique<ContinueSectionView>(
          view_delegate, kContinueColumnCount, /*tablet_mode=*/false));
  continue_section_->SetNudgeController(app_list_nudge_controller_.get());

  // Observe changes in continue section visibility, to keep separator
  // visibility in sync.
  continue_section_->AddObserver(this);

  // Recent apps row.
  recent_apps_ = scroll_contents->AddChildView(
      std::make_unique<RecentAppsView>(this, view_delegate));
  recent_apps_->UpdateAppListConfig(app_list_config);
  // Observe changes in continue section visibility, to keep separator
  // visibility in sync.
  recent_apps_->AddObserver(this);

  // Horizontal separator.
  separator_ =
      scroll_contents->AddChildView(std::make_unique<views::Separator>());
  separator_->SetBorder(views::CreateEmptyBorder(kSeparatorInsets));
  separator_->SetColor(ColorProvider::Get()->GetContentLayerColor(
      ColorProvider::ContentLayerType::kSeparatorColor));

  // Add a empty container view. A toast view should be added to
  // `toast_container_` when the app list starts temporary sorting.
  if (features::IsLauncherAppSortEnabled()) {
    toast_container_ = scroll_contents->AddChildView(
        std::make_unique<AppListToastContainerView>(
            app_list_nudge_controller_.get(), a11y_announcer,
            /*tablet_mode=*/false));
  }

  // All apps section.
  scrollable_apps_grid_view_ =
      scroll_contents->AddChildView(std::make_unique<ScrollableAppsGridView>(
          a11y_announcer, view_delegate,
          /*folder_delegate=*/nullptr, scroll_view_, folder_controller,
          /*focus_delegate=*/this));
  scrollable_apps_grid_view_->SetDragAndDropHostOfCurrentAppList(
      drag_and_drop_host);
  scrollable_apps_grid_view_->Init();
  scrollable_apps_grid_view_->UpdateAppListConfig(app_list_config);
  scrollable_apps_grid_view_->SetMaxColumns(5);
  AppListModel* const model = AppListModelProvider::Get()->model();
  scrollable_apps_grid_view_->SetModel(model);
  scrollable_apps_grid_view_->SetItemList(model->top_level_item_list());
  scrollable_apps_grid_view_->ResetForShowApps();
  // Ensure the grid fills the remaining space in the bubble so that icons can
  // be dropped beneath the last row.
  layout->SetFlexForView(scrollable_apps_grid_view_, 1);

  scroll_view_->SetContents(std::move(scroll_contents));

  UpdateSuggestions();
}

AppListBubbleAppsPage::~AppListBubbleAppsPage() {
  AppListModelProvider::Get()->RemoveObserver(this);
  continue_section_->RemoveObserver(this);
  recent_apps_->RemoveObserver(this);
}

void AppListBubbleAppsPage::UpdateSuggestions() {
  recent_apps_->ShowResults(AppListModelProvider::Get()->search_model(),
                            AppListModelProvider::Get()->model());
  continue_section_->UpdateSuggestionTasks();
  UpdateSeparatorVisibility();
}

void AppListBubbleAppsPage::AnimateShowLauncher(bool is_side_shelf) {
  DCHECK(GetVisible());

  // The animation relies on the correct positions of views, so force layout.
  if (needs_layout())
    Layout();
  DCHECK(!needs_layout());

  // This part of the animation has a longer duration than the bubble part
  // handled in AppListBubbleView, so track overall smoothness here.
  ui::AnimationThroughputReporter reporter(
      scrollable_apps_grid_view_->layer()->GetAnimator(),
      metrics_util::ForSmoothness(base::BindRepeating([](int value) {
        // This histogram name is used in Tast tests. Do not rename.
        base::UmaHistogramPercentage(
            "Apps.ClamshellLauncher.AnimationSmoothness.OpenAppsPage", value);
      })));

  // Side-shelf uses faster animations.
  const base::TimeDelta slide_duration =
      is_side_shelf ? base::Milliseconds(150) : base::Milliseconds(250);

  // Animate the views. Each section is initially offset down, then slides up
  // into its final position. For side shelf, each section is initially offset
  // up, then it slides down. If a section isn't visible, skip it. The further
  // down the section, the greater its initial offset. This code uses multiple
  // animations because views::AnimationBuilder doesn't have a good way to
  // build a single animation with conditional parts. https://crbug.com/1266020
  const int section_offset = is_side_shelf ? -20 : 20;
  int vertical_offset = 0;
  if (continue_section_->GetTasksSuggestionsCount() > 0) {
    vertical_offset += section_offset;
    SlideViewIntoPosition(continue_section_, vertical_offset, slide_duration);
  }
  if (recent_apps_->GetItemViewCount() > 0) {
    vertical_offset += section_offset;
    SlideViewIntoPosition(recent_apps_, vertical_offset, slide_duration);
  }
  if (separator_->GetVisible()) {
    // The separator is not offset; it animates next to the view above it.
    SlideViewIntoPosition(separator_, vertical_offset, slide_duration);
  }
  if (toast_container_ && toast_container_->is_toast_visible()) {
    vertical_offset += section_offset;
    SlideViewIntoPosition(toast_container_, vertical_offset, slide_duration);
  }

  // The apps grid is always visible.
  vertical_offset += section_offset;
  // Use a special cleanup callback to show the gradient mask at the end of the
  // animation. No need to use SlideViewIntoPosition() because this view always
  // has a layer.
  StartSlideInAnimation(
      scrollable_apps_grid_view_, vertical_offset, slide_duration,
      kSlideAnimationTweenType,
      base::BindRepeating(&AppListBubbleAppsPage::OnAppsGridViewAnimationEnded,
                          weak_factory_.GetWeakPtr()));
}

void AppListBubbleAppsPage::AnimateHideLauncher() {
  // Remove the gradient mask from the scroll view to improve performance.
  gradient_helper_.reset();
}

void AppListBubbleAppsPage::AnimateShowPage() {
  // If skipping animations, just update visibility.
  if (ui::ScopedAnimationDurationScaleMode::is_zero()) {
    SetVisible(true);
    return;
  }

  // Ensure any in-progress animations have their cleanup callbacks called.
  // Note that this might call SetVisible(false) from the hide animation.
  AbortAllAnimations();

  // Ensure the view is visible.
  SetVisible(true);

  // Scroll contents has a layer, so animate that.
  views::View* scroll_contents = scroll_view_->contents();
  DCHECK(scroll_contents->layer());
  DCHECK_EQ(scroll_contents->layer()->type(), ui::LAYER_TEXTURED);

  ui::AnimationThroughputReporter reporter(
      scroll_contents->layer()->GetAnimator(),
      metrics_util::ForSmoothness(base::BindRepeating([](int value) {
        base::UmaHistogramPercentage(
            "Apps.ClamshellLauncher.AnimationSmoothness.ShowAppsPage", value);
      })));

  gfx::Transform translate_down;
  translate_down.Translate(0, kShowPageAnimationVerticalOffset);

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetOpacity(scroll_contents, 0.f)
      .SetTransform(scroll_contents, translate_down)
      .At(kShowPageAnimationDelay)
      .SetDuration(kShowPageAnimationTransformDuration)
      .SetTransform(scroll_contents, gfx::Transform(),
                    gfx::Tween::LINEAR_OUT_SLOW_IN)
      .At(kShowPageAnimationDelay)
      .SetDuration(kShowPageAnimationOpacityDuration)
      .SetOpacity(scroll_contents, 1.f);
}

void AppListBubbleAppsPage::AnimateHidePage() {
  // If skipping animations, just update visibility.
  if (ui::ScopedAnimationDurationScaleMode::is_zero()) {
    SetVisible(false);
    return;
  }

  // Update view visibility when the animation is done.
  auto set_visible_false = base::BindRepeating(
      [](base::WeakPtr<AppListBubbleAppsPage> self) {
        if (!self)
          return;
        self->SetVisible(false);
        ui::Layer* layer = self->scroll_view()->contents()->layer();
        layer->SetOpacity(1.f);
        layer->SetTransform(gfx::Transform());
      },
      weak_factory_.GetWeakPtr());

  // Scroll contents has a layer, so animate that.
  views::View* scroll_contents = scroll_view_->contents();
  DCHECK(scroll_contents->layer());
  DCHECK_EQ(scroll_contents->layer()->type(), ui::LAYER_TEXTURED);

  ui::AnimationThroughputReporter reporter(
      scroll_contents->layer()->GetAnimator(),
      metrics_util::ForSmoothness(base::BindRepeating([](int value) {
        base::UmaHistogramPercentage(
            "Apps.ClamshellLauncher.AnimationSmoothness.HideAppsPage", value);
      })));

  // The animation spec says 40 dips down over 250ms, but the opacity animation
  // renders the view invisible after 50ms, so animate the visible fraction.
  gfx::Transform translate_down;
  constexpr int kVerticalOffset = 40 * 250 / 50;
  translate_down.Translate(0, kVerticalOffset);

  // Opacity: 100% -> 0%, duration 50ms
  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(set_visible_false)
      .OnAborted(set_visible_false)
      .Once()
      .SetDuration(base::Milliseconds(50))
      .SetOpacity(scroll_contents, 0.f)
      .SetTransform(scroll_contents, translate_down);
}

void AppListBubbleAppsPage::ResetScrollPosition() {
  scroll_view_->ScrollToPosition(scroll_view_->vertical_scroll_bar(), 0);
}

void AppListBubbleAppsPage::AbortAllAnimations() {
  auto abort_animations = [](views::View* view) {
    if (view->layer())
      view->layer()->GetAnimator()->AbortAllAnimations();
  };
  abort_animations(scroll_view_->contents());
  abort_animations(continue_section_);
  abort_animations(recent_apps_);
  abort_animations(separator_);
  if (toast_container_)
    abort_animations(toast_container_);
  abort_animations(scrollable_apps_grid_view_);
}

void AppListBubbleAppsPage::DisableFocusForShowingActiveFolder(bool disabled) {
  continue_section_->DisableFocusForShowingActiveFolder(disabled);
  recent_apps_->DisableFocusForShowingActiveFolder(disabled);
  scrollable_apps_grid_view_->DisableFocusForShowingActiveFolder(disabled);
}

void AppListBubbleAppsPage::UpdateForNewSortingOrder(
    const absl::optional<AppListSortOrder>& new_order,
    bool animate,
    base::OnceClosure update_position_closure) {
  DCHECK(features::IsLauncherAppSortEnabled());
  DCHECK_EQ(animate, !update_position_closure.is_null());

  // A11y announcements must happen before animations, otherwise "Search your
  // apps..." is spoken first because focus moves immediately to the search box.
  if (new_order)
    toast_container_->AnnounceSortOrder(*new_order);

  if (!animate) {
    // Reordering is not required so update the undo toast and return early.
    app_list_nudge_controller_->OnTemporarySortOrderChanged(new_order);
    toast_container_->OnTemporarySortOrderChanged(new_order);
    return;
  }

  update_position_closure_ = std::move(update_position_closure);
  scrollable_apps_grid_view_->FadeOutVisibleItemsForReorder(base::BindRepeating(
      &AppListBubbleAppsPage::OnAppsGridViewFadeOutAnimationEneded,
      weak_factory_.GetWeakPtr(), new_order));
}

bool AppListBubbleAppsPage::MaybeScrollToShowToast() {
  gfx::Point toast_origin;
  views::View::ConvertPointToTarget(toast_container_, scroll_view_->contents(),
                                    &toast_origin);
  const gfx::Rect toast_bounds_in_scroll_view =
      gfx::Rect(toast_origin, toast_container_->size());

  // Do not scroll if the toast is already fully shown.
  if (scroll_view_->GetVisibleRect().Contains(toast_bounds_in_scroll_view))
    return false;

  const int scroll_offset = separator_->GetVisible() ? separator_->y() : 0;
  scroll_view_->ScrollToPosition(scroll_view_->vertical_scroll_bar(),
                                 scroll_offset);

  return true;
}

void AppListBubbleAppsPage::Layout() {
  views::View::Layout();
  if (gradient_helper_)
    gradient_helper_->UpdateGradientZone();
}

void AppListBubbleAppsPage::VisibilityChanged(views::View* starting_from,
                                              bool is_visible) {
  // Cancel any in progress drag without running drop animation if the bubble
  // apps page is hiding.
  if (!is_visible) {
    scrollable_apps_grid_view_->CancelDragWithNoDropAnimation();
  }

  if (features::IsLauncherAppSortEnabled()) {
    // Updates the visibility state in toast container.
    AppListToastContainerView::VisibilityState state =
        is_visible ? AppListToastContainerView::VisibilityState::kShown
                   : AppListToastContainerView::VisibilityState::kHidden;
    toast_container_->UpdateVisibilityState(state);

    // Check if the reorder nudge view needs update if the bubble apps page is
    // showing.
    if (is_visible)
      toast_container_->MaybeUpdateReorderNudgeView();
  }
}

void AppListBubbleAppsPage::OnActiveAppListModelsChanged(
    AppListModel* model,
    SearchModel* search_model) {
  scrollable_apps_grid_view_->SetModel(model);
  scrollable_apps_grid_view_->SetItemList(model->top_level_item_list());

  recent_apps_->ShowResults(search_model, model);
}

void AppListBubbleAppsPage::OnViewVisibilityChanged(
    views::View* observed_view,
    views::View* starting_view) {
  if (starting_view == continue_section_ || starting_view == recent_apps_)
    UpdateSeparatorVisibility();
}

void AppListBubbleAppsPage::MoveFocusUpFromRecents() {
  DCHECK_GT(recent_apps_->GetItemViewCount(), 0);
  AppListItemView* first_recent = recent_apps_->GetItemViewAt(0);
  // Find the view one step in reverse from the first recent app.
  views::View* previous_view = GetFocusManager()->GetNextFocusableView(
      first_recent, GetWidget(), /*reverse=*/true, /*dont_loop=*/false);
  DCHECK(previous_view);
  previous_view->RequestFocus();
}

void AppListBubbleAppsPage::MoveFocusDownFromRecents(int column) {
  // When showing the sort undo toast, default to the default behavior that
  // moves focus to the toast.
  if (toast_container_ && toast_container_->is_toast_visible() &&
      toast_container_->current_toast() ==
          AppListToastContainerView::ToastType::kReorderUndo) {
    views::View* last_recent =
        recent_apps_->GetItemViewAt(recent_apps_->GetItemViewCount() - 1);
    views::View* next_view = GetFocusManager()->GetNextFocusableView(
        last_recent, GetWidget(), /*reverse=*/false, /*dont_loop=*/false);
    DCHECK(next_view);
    next_view->RequestFocus();
    return;
  }

  int top_level_item_count =
      scrollable_apps_grid_view_->view_model()->view_size();
  if (top_level_item_count <= 0)
    return;
  // Attempt to focus the item at `column` in the first row, or the last item if
  // there aren't enough items. This could happen if the user's apps are in a
  // small number of folders.
  int index = std::min(column, top_level_item_count - 1);
  AppListItemView* item = scrollable_apps_grid_view_->GetItemViewAt(index);
  DCHECK(item);
  item->RequestFocus();
}

bool AppListBubbleAppsPage::MoveFocusUpFromAppsGrid(int column) {
  DVLOG(1) << __FUNCTION__;
  // When showing the sort undo toast, default to the default behavior that
  // moves focus to the toast.
  if (toast_container_ && toast_container_->is_toast_visible() &&
      toast_container_->current_toast() ==
          AppListToastContainerView::ToastType::kReorderUndo) {
    return false;
  }

  const int recent_app_count = recent_apps_->GetItemViewCount();
  // If there aren't any recent apps, don't change focus here. Fall back to the
  // app grid's default behavior.
  if (!recent_apps_->GetVisible() || recent_app_count <= 0)
    return false;
  // Attempt to focus the item at `column`, or the last item if there aren't
  // enough items.
  int index = std::min(column, recent_app_count - 1);
  AppListItemView* item = recent_apps_->GetItemViewAt(index);
  DCHECK(item);
  item->RequestFocus();
  return true;
}

ui::Layer* AppListBubbleAppsPage::GetPageAnimationLayerForTest() {
  return scroll_view_->contents()->layer();
}

void AppListBubbleAppsPage::UpdateSeparatorVisibility() {
  separator_->SetVisible(recent_apps_->GetItemViewCount() > 0 ||
                         continue_section_->GetTasksSuggestionsCount() > 0);
}

void AppListBubbleAppsPage::DestroyLayerForView(views::View* view) {
  // This function is not static so it can be bound with a weak pointer.
  view->DestroyLayer();
}

void AppListBubbleAppsPage::OnAppsGridViewAnimationEnded() {
  // If the window is destroyed during an animation the animation will end, but
  // there's no need to build the gradient mask layer.
  if (GetWidget()->GetNativeWindow()->is_destroying())
    return;

  // Set up fade in/fade out gradients at top/bottom of scroll view. Wait until
  // the end of the show animation because the animation performs better without
  // the gradient mask layer.
  gradient_helper_ = std::make_unique<ScrollViewGradientHelper>(scroll_view_);
  gradient_helper_->UpdateGradientZone();
}

void AppListBubbleAppsPage::OnAppsGridViewFadeOutAnimationEneded(
    const absl::optional<AppListSortOrder>& new_order,
    bool aborted) {
  // Update item positions after the fade out animation but before the fade in
  // animation. NOTE: `update_position_closure_` can be empty in some edge
  // cases. For example, the app list is set with a new order denoted by Order
  // A. Then before the fade out animation is completed, the app list order is
  // reset with the old value. In this case, `update_position_closure_` for
  // Order A is never called. As a result, the closure for resetting the order
  // is empty.
  // Also update item positions only when the fade out animation ends normally.
  // Because a fade out animation is aborted when:
  // (1) Another reorder animation starts, or
  // (2) The apps grid's view model updates due to the reasons such as app
  // installation or model reset.
  // It is meaningless to update item positions in either case.
  if (update_position_closure_ && !aborted)
    std::move(update_position_closure_).Run();

  // Record the undo toast's visibility before update.
  const bool old_toast_visible = toast_container_->is_toast_visible();

  toast_container_->OnTemporarySortOrderChanged(new_order);

  // Skip the fade in animation if the fade out animation is aborted.
  if (aborted)
    return;

  const bool target_toast_visible = toast_container_->is_toast_visible();
  const bool toast_visibility_change =
      (old_toast_visible != target_toast_visible);

  // When the undo toast's visibility changes, the apps grid's bounds should
  // change. Meanwhile, the fade in animation relies on the apps grid's bounds
  // to calculate visible items. Therefore trigger layout before starting the
  // fade in animation.
  if (toast_visibility_change)
    Layout();

  // Ensure to scroll before triggering apps grid fade in animation so that
  // the bubble apps page's layout is ready.
  const bool scroll_performed = MaybeScrollToShowToast();

  scrollable_apps_grid_view_->FadeInVisibleItemsForReorder(base::BindRepeating(
      &AppListBubbleAppsPage::OnAppsGridViewFadeInAnimationEnded,
      weak_factory_.GetWeakPtr()));

  // Fade in the undo toast when:
  // (1) The toast's visibility becomes true from false, or
  // (2) The apps page is scrolled to show the toast.
  const bool should_fade_in_toast =
      target_toast_visible && (scroll_performed || toast_visibility_change);

  if (!should_fade_in_toast)
    return;

  // If the undo toast does not have its layer before fade in animation,
  // create one.
  bool has_layer_before_animation = toast_container_->layer();
  if (!has_layer_before_animation) {
    toast_container_->SetPaintToLayer();
    toast_container_->layer()->SetFillsBoundsOpaquely(false);
  }

  // Hide the undo toast instantly before starting the toast fade in animation.
  toast_container_->layer()->SetOpacity(0.f);

  views::AnimationBuilder animation_builder;
  toast_fade_in_abort_handle_ = animation_builder.GetAbortHandle();
  animation_builder
      .OnEnded(base::BindOnce(
          &AppListBubbleAppsPage::OnReorderUndoToastFadeInAnimationEnded,
          weak_factory_.GetWeakPtr(),
          /*aborted=*/false, has_layer_before_animation))
      .OnAborted(base::BindOnce(
          &AppListBubbleAppsPage::OnReorderUndoToastFadeInAnimationEnded,
          weak_factory_.GetWeakPtr(),
          /*aborted=*/true, has_layer_before_animation))
      .Once()
      .SetDuration(kToastFadeInAnimationDuration)
      .SetOpacity(toast_container_->layer(), 1.f);
}

void AppListBubbleAppsPage::OnAppsGridViewFadeInAnimationEnded(bool aborted) {
  if (!aborted)
    return;

  // Abort the toast fade in animation if the apps grid fade in animation is
  // aborted.
  toast_fade_in_abort_handle_.reset();
}

void AppListBubbleAppsPage::OnReorderUndoToastFadeInAnimationEnded(
    bool aborted,
    bool clean_layer) {
  toast_fade_in_abort_handle_.reset();

  if (aborted && !clean_layer) {
    // Ensure that the toast shows when the animation is aborted.
    toast_container_->layer()->SetOpacity(1.f);
    return;
  }

  if (clean_layer) {
    toast_container_->DestroyLayer();
    return;
  }
}

void AppListBubbleAppsPage::SlideViewIntoPosition(views::View* view,
                                                  int vertical_offset,
                                                  base::TimeDelta duration) {
  // Animation spec:
  //
  // Y Position: Down (offset) → End position
  // Ease: (0.00, 0.00, 0.20, 1.00)

  const bool create_layer = PrepareForLayerAnimation(view);

  // If we created a layer for the view, undo that when the animation ends.
  // The underlying views don't expose weak pointers directly, so use a weak
  // pointer to this view, which owns its children.
  auto cleanup = create_layer ? base::BindRepeating(
                                    &AppListBubbleAppsPage::DestroyLayerForView,
                                    weak_factory_.GetWeakPtr(), view)
                              : base::DoNothing();
  StartSlideInAnimation(view, vertical_offset, duration,
                        kSlideAnimationTweenType, cleanup);
}

BEGIN_METADATA(AppListBubbleAppsPage, views::View)
END_METADATA

}  // namespace ash
