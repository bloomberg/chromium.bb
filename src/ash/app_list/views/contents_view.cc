// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/contents_view.h"

#include <algorithm>
#include <vector>

#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/views/app_list_folder_view.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/apps_container_view.h"
#include "ash/app_list/views/apps_grid_view.h"
#include "ash/app_list/views/assistant/assistant_page_view.h"
#include "ash/app_list/views/expand_arrow_view.h"
#include "ash/app_list/views/paged_apps_grid_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/app_list/views/search_result_list_view.h"
#include "ash/app_list/views/search_result_page_view.h"
#include "ash/app_list/views/search_result_tile_item_list_view.h"
#include "ash/constants/ash_features.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_switches.h"
#include "base/bind.h"
#include "base/check_op.h"
#include "base/cxx17_backports.h"
#include "base/notreached.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/view_constants_aura.h"
#include "ui/views/view_model.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// The contents view height threshold under which search box should be shown in
// dense layout.
constexpr int kDenseLayoutHeightThreshold = 600;

// The preferred search box height.
constexpr int kSearchBoxHeight = 48;

// The preferred search box height when the vertical app list contents space
// is condensed - normally `kSearchBoxHeight` would be used.
constexpr int kSearchBoxHeightForDenseLayout = 40;

// The top search box margin (measured from the app list view top bound) when
// app list view is in peeking/half state on non-apps page.
constexpr int kDefaultSearchBoxTopMarginInPeekingState = 24;

// The top search box margin (measured from the app list view top bound) when
// app list view is in peeking state on the apps page.
constexpr int kSearchBoxTopMarginInPeekingAppsPage = 84;

// The range of app list transition progress in which the expand arrow'
// opacity changes from 0 to 1.
constexpr float kExpandArrowOpacityStartProgress = 0.61;
constexpr float kExpandArrowOpacityEndProgress = 1;
constexpr int kSearchBarMinWidth = 440;

// Range of the fraction of app list from collapsed to peeking that search box
// should change opacity.
constexpr float kSearchBoxOpacityStartProgress = 0.11f;
constexpr float kSearchBoxOpacityEndProgress = 1.0f;

// Duration for page transition.
constexpr base::TimeDelta kPageTransitionDuration = base::Milliseconds(250);

// Duration for overscroll page transition.
constexpr base::TimeDelta kOverscrollPageTransitionDuration =
    base::Milliseconds(50);

// Calculates opacity value for the current app list progress.
// |progress| - The target app list view progress - a value in [0.0, 2.0]
//              interval that describes the app list view position relative to
//              peeking and fullscreen state.
// |transition_start| - The app list view progress at which opacity equals 0.0.
// |transition_end| - The app list view progress at which opacity equals 1.0.
float GetOpacityForProgress(float progress,
                            float transition_start,
                            float transition_end) {
  return base::clamp(
      (progress - transition_start) / (transition_end - transition_start), 0.0f,
      1.0f);
}

}  // namespace

ContentsView::ContentsView(AppListView* app_list_view)
    : app_list_view_(app_list_view) {
  pagination_model_.SetTransitionDurations(kPageTransitionDuration,
                                           kOverscrollPageTransitionDuration);
  pagination_model_.AddObserver(this);
}

ContentsView::~ContentsView() {
  pagination_model_.RemoveObserver(this);
}

// static
int ContentsView::GetPeekingSearchBoxTopMarginOnPage(AppListState page) {
  return page == AppListState::kStateApps
             ? kSearchBoxTopMarginInPeekingAppsPage
             : kDefaultSearchBoxTopMarginInPeekingState;
}

void ContentsView::Init() {
  AppListViewDelegate* view_delegate = GetAppListMainView()->view_delegate();
  apps_container_view_ = AddLauncherPage(
      std::make_unique<AppsContainerView>(this), AppListState::kStateApps);

  // Search results UI.
  auto search_result_page_view = std::make_unique<SearchResultPageView>();
  search_result_page_view->InitializeContainers(
      view_delegate, GetAppListMainView(), GetSearchBoxView());

  expand_arrow_view_ =
      AddChildView(std::make_unique<ExpandArrowView>(this, app_list_view_));

  search_result_page_view_ = AddLauncherPage(std::move(search_result_page_view),
                                             AppListState::kStateSearchResults);

  auto assistant_page_view = std::make_unique<AssistantPageView>(
      view_delegate->GetAssistantViewDelegate());
  assistant_page_view->SetVisible(false);
  assistant_page_view_ = AddLauncherPage(std::move(assistant_page_view),
                                         AppListState::kStateEmbeddedAssistant);

  int initial_page_index = GetPageIndexForState(AppListState::kStateApps);
  DCHECK_GE(initial_page_index, 0);

  page_before_search_ = initial_page_index;
  // Must only call SetTotalPages once all the launcher pages have been added
  // (as it will trigger a SelectedPageChanged call).
  pagination_model_.SetTotalPages(app_list_pages_.size());

  // Page 0 is selected by SetTotalPages and needs to be 'hidden' when selecting
  // the initial page.
  app_list_pages_[GetActivePageIndex()]->OnWillBeHidden();

  pagination_model_.SelectPage(initial_page_index, false);

  // Update suggestion chips after valid page is selected to prevent the update
  // from being ignored.
  apps_container_view_->UpdateSuggestionChips();

  ActivePageChanged();

  // Hide the search results initially.
  ShowSearchResults(false);
}

void ContentsView::ResetForShow() {
  target_page_for_last_view_state_update_ = absl::nullopt;
  apps_container_view_->ResetForShowApps();
  // SearchBoxView::ResetForShow() before SetActiveState(). It clears the search
  // query internally, which can show the search results page through
  // QueryChanged(). Since it wants to reset to kStateApps, first reset the
  // search box and then set its active state to kStateApps.
  GetSearchBoxView()->ResetForShow();
  // Make sure the default visibilities of the pages. This should be done before
  // SetActiveState() since it checks the visibility of the pages.
  apps_container_view_->SetVisible(true);
  search_result_page_view_->SetVisible(false);
  if (assistant_page_view_)
    assistant_page_view_->SetVisible(false);
  SetActiveState(AppListState::kStateApps, /*animate=*/false);
  // In side shelf, the opacity of the contents is not animated so set it to the
  // final state. In tablet mode, opacity of the elements is controlled by the
  // AppListControllerImpl which expects these elements to be opaque.
  // Otherwise the contents animate from 0 to 1 so set the initial opacity to 0.
  if (app_list_view_->is_side_shelf() || app_list_view_->is_tablet_mode()) {
    AnimateToViewState(AppListViewState::kFullscreenAllApps, base::TimeDelta());
  } else if (!last_target_view_state_.has_value() ||
             *last_target_view_state_ != AppListViewState::kClosed) {
    AnimateToViewState(AppListViewState::kClosed, base::TimeDelta());
  }
}

void ContentsView::CancelDrag() {
  if (apps_container_view_->apps_grid_view()->has_dragged_item())
    apps_container_view_->apps_grid_view()->EndDrag(true);
  if (apps_container_view_->app_list_folder_view()
          ->items_grid_view()
          ->has_dragged_item()) {
    apps_container_view_->app_list_folder_view()->items_grid_view()->EndDrag(
        true);
  }
}

void ContentsView::SetDragAndDropHostOfCurrentAppList(
    ApplicationDragAndDropHost* drag_and_drop_host) {
  apps_container_view_->SetDragAndDropHostOfCurrentAppList(drag_and_drop_host);
}

void ContentsView::OnAppListViewTargetStateChanged(
    AppListViewState target_state) {
  target_view_state_ = target_state;
  if (target_state == AppListViewState::kClosed) {
    CancelDrag();
    expand_arrow_view_->MaybeEnableHintingAnimation(false);
    return;
  }
  UpdateExpandArrowBehavior(target_state);
}

void ContentsView::OnTabletModeChanged(bool started) {
  apps_container_view_->OnTabletModeChanged(started);

  UpdateExpandArrowOpacity(GetActiveState(), target_view_state(),
                           kPageTransitionDuration);
}

void ContentsView::SetActiveState(AppListState state) {
  SetActiveState(state, true /*animate*/);
}

void ContentsView::SetActiveState(AppListState state, bool animate) {
  if (IsStateActive(state))
    return;

  // The primary way to set the state to search or Assistant results should be
  // via |ShowSearchResults| or |ShowEmbeddedAssistantUI|.
  DCHECK(state != AppListState::kStateSearchResults &&
         state != AppListState::kStateEmbeddedAssistant);

  const int page_index = GetPageIndexForState(state);
  page_before_search_ = page_index;
  SetActiveStateInternal(page_index, animate);
}

int ContentsView::GetActivePageIndex() const {
  // The active page is changed at the beginning of an animation, not the end.
  return pagination_model_.SelectedTargetPage();
}

AppListState ContentsView::GetActiveState() const {
  return GetStateForPageIndex(GetActivePageIndex());
}

bool ContentsView::IsStateActive(AppListState state) const {
  int active_page_index = GetActivePageIndex();
  return active_page_index >= 0 &&
         GetPageIndexForState(state) == active_page_index;
}

int ContentsView::GetPageIndexForState(AppListState state) const {
  // Find the index of the view corresponding to the given state.
  std::map<AppListState, int>::const_iterator it = state_to_view_.find(state);
  if (it == state_to_view_.end())
    return -1;

  return it->second;
}

AppListState ContentsView::GetStateForPageIndex(int index) const {
  std::map<int, AppListState>::const_iterator it = view_to_state_.find(index);
  if (it == view_to_state_.end())
    return AppListState::kInvalidState;

  return it->second;
}

int ContentsView::NumLauncherPages() const {
  return pagination_model_.total_pages();
}

gfx::Size ContentsView::AdjustSearchBoxSizeToFitMargins(
    const gfx::Size& preferred_size) const {
  const int padded_width = GetContentsBounds().width() -
                           2 * apps_container_view_->GetIdealHorizontalMargin();
  return gfx::Size(
      base::clamp(padded_width, kSearchBarMinWidth, preferred_size.width()),
      preferred_size.height());
}

void ContentsView::SetActiveStateInternal(int page_index, bool animate) {
  if (!GetPageView(page_index)->GetVisible())
    return;

  app_list_pages_[GetActivePageIndex()]->OnWillBeHidden();

  // Start animating to the new page. Disable animation for tests.
  bool should_animate = animate && !set_active_state_without_animation_ &&
                        !ui::ScopedAnimationDurationScaleMode::is_zero();

  // There's a chance of selecting page during the transition animation. To
  // reschedule the new animation from the beginning, |pagination_model_| needs
  // to finish the ongoing animation here.
  if (should_animate && pagination_model_.has_transition() &&
      pagination_model_.transition().target_page != page_index) {
    pagination_model_.FinishAnimation();
    // If the pending animation was animating from the current target page, the
    // target page might have got hidden as the animation was finished. Make
    // sure the page is reshown in that case.
    GetPageView(page_index)->SetVisible(true);
  }
  pagination_model_.SelectPage(page_index, should_animate);
  ActivePageChanged();
  UpdateExpandArrowOpacity(
      GetActiveState(), target_view_state(),
      should_animate ? kPageTransitionDuration : base::TimeDelta());

  if (!should_animate)
    Layout();
}

void ContentsView::ActivePageChanged() {
  AppListState state = AppListState::kInvalidState;

  std::map<int, AppListState>::const_iterator it =
      view_to_state_.find(GetActivePageIndex());
  if (it != view_to_state_.end())
    state = it->second;

  app_list_pages_[GetActivePageIndex()]->OnWillBeShown();

  GetAppListMainView()->view_delegate()->OnAppListPageChanged(state);
  UpdateSearchBoxVisibility(state);
  app_list_view_->UpdateWindowTitle();
}

void ContentsView::ShowSearchResults(bool show) {
  int search_page = GetPageIndexForState(AppListState::kStateSearchResults);
  DCHECK_GE(search_page, 0);

  // Hide or Show results
  search_result_page_view()->SetVisible(show);
  SetActiveStateInternal(show ? search_page : page_before_search_,
                         true /*animate*/);
  if (show)
    search_result_page_view()->UpdateResultContainersVisibility();
}

bool ContentsView::IsShowingSearchResults() const {
  return IsStateActive(AppListState::kStateSearchResults);
}

void ContentsView::ShowEmbeddedAssistantUI(bool show) {
  const int assistant_page =
      GetPageIndexForState(AppListState::kStateEmbeddedAssistant);
  DCHECK_GE(assistant_page, 0);

  const int current_page = pagination_model_.SelectedTargetPage();
  // When closing the Assistant UI we return to the last page before the
  // search box.
  const int next_page = show ? assistant_page : page_before_search_;

  // Show or hide results.
  if (current_page != next_page) {
    GetPageView(current_page)->SetVisible(false);
    GetPageView(next_page)->SetVisible(true);
  }

  SetActiveStateInternal(next_page, true /*animate*/);
  // Sometimes the page stays in |assistant_page|, but the preferred bounds
  // might change meanwhile.
  if (show && current_page == assistant_page) {
    GetPageView(assistant_page)
        ->UpdatePageBoundsForState(
            AppListState::kStateEmbeddedAssistant, GetContentsBounds(),
            GetSearchBoxBounds(AppListState::kStateEmbeddedAssistant));
  }
  // If |next_page| is kStateApps, we need to set app_list_view to
  // kPeeking and layout the suggestion chips.
  if (next_page == GetPageIndexForState(AppListState::kStateApps)) {
    GetSearchBoxView()->ClearSearch();
    GetSearchBoxView()->SetSearchBoxActive(false, ui::ET_UNKNOWN);
    apps_container_view_->Layout();
  }
}

bool ContentsView::IsShowingEmbeddedAssistantUI() const {
  return IsStateActive(AppListState::kStateEmbeddedAssistant);
}

void ContentsView::InitializeSearchBoxAnimation(AppListState current_state,
                                                AppListState target_state) {
  SearchBoxView* search_box = GetSearchBoxView();
  if (!search_box->GetWidget())
    return;

  search_box->UpdateLayout(target_state,
                           GetSearchBoxSize(target_state).height());
  search_box->UpdateBackground(target_state);

  gfx::Rect target_bounds = GetSearchBoxBounds(target_state);
  target_bounds = search_box->GetViewBoundsForSearchBoxContentsBounds(
      ConvertRectToWidgetWithoutTransform(target_bounds));

  // The search box animation is conducted as transform animation. Initially
  // search box changes its bounds to the target bounds but sets the transform
  // to be original bounds. Note that this transform shouldn't be animated
  // through ui::LayerAnimator since intermediate transformed bounds might not
  // match with other animation and that could look janky.
  search_box->GetWidget()->SetBounds(target_bounds);

  UpdateSearchBoxAnimation(0.0f, current_state, target_state);
}

void ContentsView::UpdateSearchBoxAnimation(double progress,
                                            AppListState current_state,
                                            AppListState target_state) {
  SearchBoxView* search_box = GetSearchBoxView();
  if (!search_box->GetWidget())
    return;

  gfx::Rect previous_bounds = GetSearchBoxBounds(current_state);
  previous_bounds = search_box->GetViewBoundsForSearchBoxContentsBounds(
      ConvertRectToWidgetWithoutTransform(previous_bounds));
  gfx::Rect target_bounds = GetSearchBoxBounds(target_state);
  target_bounds = search_box->GetViewBoundsForSearchBoxContentsBounds(
      ConvertRectToWidgetWithoutTransform(target_bounds));

  gfx::Rect current_bounds =
      gfx::Tween::RectValueBetween(progress, previous_bounds, target_bounds);
  gfx::Transform transform;

  if (current_bounds != target_bounds) {
    transform.Translate(current_bounds.origin() - target_bounds.origin());
    transform.Scale(
        static_cast<float>(current_bounds.width()) / target_bounds.width(),
        static_cast<float>(current_bounds.height()) / target_bounds.height());
  }
  search_box->GetWidget()->GetLayer()->SetTransform(transform);

  // Update search box view layer.
  const float current_radius =
      search_box->GetSearchBoxBorderCornerRadiusForState(current_state);
  const float target_radius =
      search_box->GetSearchBoxBorderCornerRadiusForState(target_state);
  search_box->layer()->SetClipRect(search_box->GetContentsBounds());
  search_box->layer()->SetRoundedCornerRadius(gfx::RoundedCornersF(
      gfx::Tween::FloatValueBetween(progress, current_radius, target_radius)));
}

void ContentsView::UpdateExpandArrowBehavior(AppListViewState target_state) {
  const bool expand_arrow_enabled = target_state == AppListViewState::kPeeking;
  // The expand arrow is only focusable and has InkDropMode on in peeking
  // state.
  expand_arrow_view_->SetFocusBehavior(
      expand_arrow_enabled ? FocusBehavior::ALWAYS : FocusBehavior::NEVER);
  views::InkDrop::Get(expand_arrow_view_)
      ->SetMode(expand_arrow_enabled ? views::InkDropHost::InkDropMode::ON
                                     : views::InkDropHost::InkDropMode::OFF);

  // Allow ChromeVox to focus the expand arrow only when peeking launcher.
  expand_arrow_view_->GetViewAccessibility().OverrideIsIgnored(
      !expand_arrow_enabled);
  expand_arrow_view_->NotifyAccessibilityEvent(ax::mojom::Event::kTreeChanged,
                                               true);

  expand_arrow_view_->MaybeEnableHintingAnimation(expand_arrow_enabled);
}

void ContentsView::UpdateExpandArrowOpacity(
    AppListState target_state,
    AppListViewState target_app_list_view_state,
    base::TimeDelta transition_duration) {
  const bool target_visibility =
      target_state == AppListState::kStateApps &&
      target_app_list_view_state != AppListViewState::kClosed &&
      !app_list_view_->is_side_shelf() && !app_list_view_->is_tablet_mode();

  float target_opacity = target_visibility ? 1.0f : 0.0f;
  // Update the target opacity for when the app list view is in drag.
  if (target_opacity && app_list_view_->is_in_drag()) {
    // NOTE: The expand arrow is only shown for apps page, so it's OK to use the
    // app list drag progress for apps page.
    const float progress = app_list_view_->GetAppListTransitionProgress(
        AppListView::kProgressFlagNone);
    target_opacity =
        GetOpacityForProgress(progress, kExpandArrowOpacityStartProgress,
                              kExpandArrowOpacityEndProgress);
  }

  ui::Layer* const layer = expand_arrow_view_->layer();
  if (transition_duration.is_zero()) {
    layer->SetOpacity(target_opacity);
    return;
  }

  if (target_opacity == layer->GetTargetOpacity())
    return;

  ui::ScopedLayerAnimationSettings animation(layer->GetAnimator());
  animation.SetTransitionDuration(transition_duration);
  animation.SetTweenType(gfx::Tween::EASE_IN);
  animation.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  layer->SetOpacity(target_opacity);
}

void ContentsView::UpdateSearchBoxVisibility(AppListState current_state) {
  auto* search_box_widget = GetSearchBoxView()->GetWidget();
  if (search_box_widget) {
    // Hide search box widget in order to click on the embedded Assistant UI.
    const bool show_search_box =
        current_state != AppListState::kStateEmbeddedAssistant;
    show_search_box ? search_box_widget->Show() : search_box_widget->Hide();
  }
}

AppListPage* ContentsView::GetPageView(int index) const {
  DCHECK_GT(static_cast<int>(app_list_pages_.size()), index);
  return app_list_pages_[index];
}

SearchBoxView* ContentsView::GetSearchBoxView() const {
  return GetAppListMainView()->search_box_view();
}

AppListMainView* ContentsView::GetAppListMainView() const {
  return app_list_view_->app_list_main_view();
}

void ContentsView::AddLauncherPageInternal(std::unique_ptr<AppListPage> view,
                                           AppListState state) {
  view->set_contents_view(this);
  app_list_pages_.push_back(AddChildView(std::move(view)));
  int page_index = app_list_pages_.size() - 1;
  bool success =
      state_to_view_.insert(std::make_pair(state, page_index)).second;
  success = success &&
            view_to_state_.insert(std::make_pair(page_index, state)).second;

  // There shouldn't be duplicates in either map.
  DCHECK(success);
}

gfx::Rect ContentsView::GetSearchBoxBounds(AppListState state) const {
  if (app_list_view_->is_in_drag()) {
    return GetSearchBoxExpectedBoundsForProgress(
        state, app_list_view_->GetAppListTransitionProgress(
                   state == AppListState::kStateSearchResults
                       ? AppListView::kProgressFlagSearchResults
                       : AppListView::kProgressFlagNone));
  }
  return GetSearchBoxBoundsForViewState(state, target_view_state());
}

gfx::Size ContentsView::GetSearchBoxSize(AppListState state) const {
  AppListPage* page = GetPageView(GetPageIndexForState(state));
  gfx::Size size_preferred_by_page = page->GetPreferredSearchBoxSize();
  if (!size_preferred_by_page.IsEmpty())
    return AdjustSearchBoxSizeToFitMargins(size_preferred_by_page);

  gfx::Size preferred_size = GetSearchBoxView()->GetPreferredSize();

  // Reduce the search box size in fullscreen view state when the work area
  // height is less than 600 dip - the goal is to increase the amount of space
  // available to the apps grid.
  if (!features::IsProductivityLauncherEnabled() &&
      GetContentsBounds().height() < kDenseLayoutHeightThreshold) {
    preferred_size.set_height(kSearchBoxHeightForDenseLayout);
  } else {
    preferred_size.set_height(kSearchBoxHeight);
  }

  return AdjustSearchBoxSizeToFitMargins(preferred_size);
}

gfx::Rect ContentsView::GetSearchBoxBoundsForViewState(
    AppListState state,
    AppListViewState view_state) const {
  gfx::Size size = GetSearchBoxSize(state);
  return gfx::Rect(gfx::Point((width() - size.width()) / 2,
                              GetSearchBoxTopForViewState(state, view_state)),
                   size);
}

gfx::Rect ContentsView::GetSearchBoxExpectedBoundsForProgress(
    AppListState state,
    float progress) const {
  AppListViewState baseline_state = state == AppListState::kStateSearchResults
                                        ? AppListViewState::kHalf
                                        : AppListViewState::kPeeking;
  gfx::Rect bounds = GetSearchBoxBoundsForViewState(state, baseline_state);

  if (progress <= 1) {
    bounds.set_y(gfx::Tween::IntValueBetween(progress, 0, bounds.y()));
  } else {
    const int fullscreen_y = GetSearchBoxTopForViewState(
        state, AppListViewState::kFullscreenAllApps);
    bounds.set_y(
        gfx::Tween::IntValueBetween(progress - 1, bounds.y(), fullscreen_y));
  }
  return bounds;
}

bool ContentsView::Back() {
  // If the virtual keyboard is visible, dismiss the keyboard and return early
  auto* const keyboard_controller = keyboard::KeyboardUIController::Get();
  if (keyboard_controller->IsKeyboardVisible()) {
    keyboard_controller->HideKeyboardByUser();
    return true;
  }

  AppListState state = view_to_state_[GetActivePageIndex()];
  switch (state) {
    case AppListState::kStateApps: {
      PaginationModel* pagination_model =
          apps_container_view_->apps_grid_view()->pagination_model();
      if (apps_container_view_->IsInFolderView()) {
        apps_container_view_->app_list_folder_view()->CloseFolderPage();
      } else if (app_list_view_->is_tablet_mode() &&
                 pagination_model->total_pages() > 0 &&
                 pagination_model->selected_page() > 0) {
        bool animate = !ui::ScopedAnimationDurationScaleMode::is_zero();
        pagination_model->SelectPage(0, animate);
      } else {
        // Close the app list when Back() is called from the apps page.
        return false;
      }
      break;
    }
    case AppListState::kStateSearchResults:
      GetSearchBoxView()->ClearSearchAndDeactivateSearchBox();
      ShowSearchResults(false);
      break;
    case AppListState::kStateEmbeddedAssistant:
      ShowEmbeddedAssistantUI(false);
      break;
    case AppListState::kStateStart_DEPRECATED:
    case AppListState::kInvalidState:
      NOTREACHED();
      break;
  }
  return true;
}

void ContentsView::Layout() {
  const gfx::Rect rect = GetContentsBounds();
  if (rect.IsEmpty())
    return;

  // Layout expand arrow.
  gfx::Rect arrow_rect(GetContentsBounds());
  const gfx::Size arrow_size(expand_arrow_view_->GetPreferredSize());
  arrow_rect.set_height(arrow_size.height());
  arrow_rect.ClampToCenteredSize(arrow_size);
  expand_arrow_view_->SetBoundsRect(arrow_rect);
  expand_arrow_view_->SchedulePaint();

  if (pagination_model_.has_transition())
    return;

  UpdateYPositionAndOpacity();

  const AppListState current_state =
      GetStateForPageIndex(pagination_model_.selected_page());
  SearchBoxView* const search_box = GetSearchBoxView();
  const int search_box_height = GetSearchBoxSize(current_state).height();
  search_box->UpdateLayout(current_state, search_box_height);
  search_box->UpdateBackground(current_state);

  // Reset the transform which can be set through animation
  search_box->GetWidget()->GetLayer()->SetTransform(gfx::Transform());
}

const char* ContentsView::GetClassName() const {
  return "ContentsView";
}

void ContentsView::TotalPagesChanged(int previous_page_count,
                                     int new_page_count) {}

void ContentsView::SelectedPageChanged(int old_selected, int new_selected) {
  if (old_selected >= 0)
    app_list_pages_[old_selected]->OnHidden();

  if (new_selected >= 0)
    app_list_pages_[new_selected]->OnShown();
}

void ContentsView::TransitionStarted() {
  const int current_page = pagination_model_.selected_page();
  const int target_page = pagination_model_.transition().target_page;

  const AppListState current_state = GetStateForPageIndex(current_page);
  const AppListState target_state = GetStateForPageIndex(target_page);
  for (AppListPage* page : app_list_pages_)
    page->OnAnimationStarted(current_state, target_state);

  InitializeSearchBoxAnimation(current_state, target_state);
}

void ContentsView::TransitionChanged() {
  const int current_page = pagination_model_.selected_page();
  const int target_page = pagination_model_.transition().target_page;

  const AppListState current_state = GetStateForPageIndex(current_page);
  const AppListState target_state = GetStateForPageIndex(target_page);
  const double progress = pagination_model_.transition().progress;
  for (AppListPage* page : app_list_pages_) {
    if (!page->GetVisible() ||
        !ShouldLayoutPage(page, current_state, target_state)) {
      continue;
    }
    page->OnAnimationUpdated(progress, current_state, target_state);
  }

  // Update search box's transform gradually. See the comment in
  // InitiateSearchBoxAnimation for why it's not animated through
  // ui::LayerAnimator.
  UpdateSearchBoxAnimation(progress, current_state, target_state);
}

void ContentsView::UpdateYPositionAndOpacity() {
  const int current_page = pagination_model_.has_transition()
                               ? pagination_model_.transition().target_page
                               : pagination_model_.selected_page();
  const AppListState current_state = GetStateForPageIndex(current_page);
  UpdateExpandArrowOpacity(current_state, target_view_state(),
                           base::TimeDelta());
  expand_arrow_view_->SchedulePaint();

  SearchBoxView* search_box = GetSearchBoxView();
  const gfx::Rect search_box_bounds = GetSearchBoxBounds(current_state);
  const gfx::Rect search_rect =
      search_box->GetViewBoundsForSearchBoxContentsBounds(
          ConvertRectToWidgetWithoutTransform(search_box_bounds));
  search_box->GetWidget()->SetBounds(search_rect);

  float progress = 0.0f;
  if (app_list_view_->is_in_drag()) {
    progress = app_list_view_->GetAppListTransitionProgress(
        current_state == AppListState::kStateSearchResults
            ? AppListView::kProgressFlagSearchResults
            : AppListView::kProgressFlagNone);
  } else {
    progress = AppListView::GetTransitionProgressForState(target_view_state());
  }
  const bool restore_opacity = !app_list_view_->is_in_drag() &&
                               target_view_state() != AppListViewState::kClosed;
  const float search_box_opacity =
      restore_opacity
          ? 1.0f
          : GetOpacityForProgress(progress, kSearchBoxOpacityStartProgress,
                                  kSearchBoxOpacityEndProgress);
  search_box->layer()->SetOpacity(search_box_opacity);

  for (AppListPage* page : app_list_pages_) {
    page->UpdatePageBoundsForState(current_state, GetContentsBounds(),
                                   search_box_bounds);
    page->UpdatePageOpacityForState(current_state, search_box_opacity,
                                    restore_opacity);
  }

  // If in drag, reset the transforms that might have been set in
  // AnimateToViewState().
  if (app_list_view_->is_in_drag()) {
    search_box->layer()->SetTransform(gfx::Transform());
    expand_arrow_view_->layer()->SetTransform(gfx::Transform());
  }

  target_page_for_last_view_state_update_ = current_state;
}

void ContentsView::AnimateToViewState(AppListViewState target_view_state,
                                      const base::TimeDelta& duration) {
  const AppListState target_page =
      GetStateForPageIndex(pagination_model_.has_transition()
                               ? pagination_model_.transition().target_page
                               : pagination_model_.selected_page());

  // Animates layer's opacity.
  // |duration| - The default transition duration. The actual transition gets
  //     halved when animating to hidden state.
  // |view| - The view to animate.
  // |target_visibility| - The target layer visibility.
  auto animate_opacity = [](base::TimeDelta duration, views::View* view,
                            bool target_visibility) {
    ui::Layer* const layer = view->layer();
    ui::ScopedLayerAnimationSettings animation(layer->GetAnimator());
    // Speed up the search box and contents view animation when closing the app
    // list, so they are not visible when the app list moves under the shelf.
    animation.SetTransitionDuration(duration / (target_visibility ? 1 : 2));
    animation.SetTweenType(gfx::Tween::EASE_IN);
    animation.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    layer->SetOpacity(target_visibility ? 1.0f : 0.0f);
  };

  // Fade in or out the contents view, the search box.
  const bool closing = target_view_state == AppListViewState::kClosed;
  animate_opacity(duration, GetSearchBoxView(), !closing /*target_visibility*/);

  // Fade in or out the expand arrow. Speed up the animation when closing the
  // app list to match the animation duration used for search box - see
  // `animate_opacity()`.
  UpdateExpandArrowOpacity(target_page, target_view_state,
                           duration / (closing ? 2 : 1));

  // Animates layer's vertical position (using transform animation).
  // |layer| - The layer to transform.
  // |y_offset| - The initial vertical offset - the layer's vertical offset will
  //              be animated to 0.
  auto animate_transform = [](base::TimeDelta duration, float y_offset,
                              ui::Layer* layer) {
    gfx::Transform transform;
    transform.Translate(0, y_offset);
    layer->SetTransform(transform);

    auto settings = std::make_unique<ui::ScopedLayerAnimationSettings>(
        layer->GetAnimator());
    settings->SetTweenType(gfx::Tween::EASE_OUT);
    settings->SetTransitionDuration(duration);
    settings->SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET);
    layer->SetTransform(gfx::Transform());
  };

  // Animate the app list contents to the target state. The transform is
  // performed by setting the target view bounds (for search box and the app
  // list pages), applying a transform that positions the views into their
  // current states, and finally setting up transform animation to the identity
  // transform (to move the layers into their target bounds).
  // Note that pages are positioned relative to the search box view, and the
  // vertical offset from the search box remains constant through out the
  // animation, so it's sufficient to calculate the target search box view
  // offset and apply the transform to the whole contents view.
  const gfx::Rect target_search_box_bounds =
      GetSearchBoxBoundsForViewState(target_page, target_view_state);

  SearchBoxView* search_box = GetSearchBoxView();
  const gfx::Rect target_search_box_widget_bounds =
      search_box->GetViewBoundsForSearchBoxContentsBounds(
          ConvertRectToWidgetWithoutTransform(target_search_box_bounds));
  search_box->GetWidget()->SetBounds(target_search_box_widget_bounds);

  // Even though the target bounds are calculated for the target page, use the
  // last page for which app list view state was updated - in case page
  // transition is in progress, the total search box position change can be
  // described as composite of:
  // 1.  Change in contents view padding due to app list view state change.
  // 2.  Change in contents view padding due to page change.
  // Only the first part is expected to be handled by this animation, and this
  // uses the last used page as reference.
  // The second change will be handled by the page transition animation.
  const AppListState selected_page =
      target_page_for_last_view_state_update_.value_or(
          GetStateForPageIndex(pagination_model_.selected_page()));
  const int progress_baseline_flag =
      selected_page == AppListState::kStateSearchResults
          ? AppListView::kProgressFlagSearchResults
          : AppListView::kProgressFlagNone;
  const float progress = app_list_view_->GetAppListTransitionProgress(
      AppListView::kProgressFlagWithTransform | progress_baseline_flag);
  const gfx::Rect current_search_box_bounds =
      GetSearchBoxExpectedBoundsForProgress(selected_page, progress);

  const int y_offset =
      current_search_box_bounds.y() -
      GetSearchBoxBoundsForViewState(selected_page, target_view_state).y();

  // For search box, animate the search_box view layer instead of the widget
  // layer to avoid conflict with pagination model transitions (which update the
  // search box widget layer transform as the transition progresses).
  animate_transform(duration, y_offset, search_box->layer());

  // Update app list page bounds to their target values. This assumes that
  // potential in-progress pagination transition does not directly animate page
  // bounds.
  for (AppListPage* page : app_list_pages_) {
    page->UpdatePageBoundsForState(target_page, GetContentsBounds(),
                                   target_search_box_bounds);

    page->AnimateOpacity(progress, target_view_state,
                         base::BindRepeating(animate_opacity, duration));
    page->AnimateYPosition(target_view_state,
                           base::BindRepeating(animate_transform, duration),
                           y_offset);
  }

  // Assistant page and search results page may host native views (e.g. for
  // card assistant results). These windows are descendants of the app list
  // view window layer rather than the page layers, so they have to be
  // animated separately from their associated page.
  for (auto* child_window : GetWidget()->GetNativeWindow()->children()) {
    View* host_view = child_window->GetProperty(views::kHostViewKey);
    if (!host_view)
      continue;
    animate_transform(duration, y_offset, child_window->layer());
  }

  last_target_view_state_ = target_view_state;
  target_page_for_last_view_state_update_ = target_page;

  // Schedule expand arrow repaint to ensure the view picks up the new target
  // state.
  if (target_view_state != AppListViewState::kClosed)
    expand_arrow_view()->SchedulePaint();
  animate_transform(
      duration,
      expand_arrow_view()->CalculateOffsetFromCurrentAppListProgress(progress),
      expand_arrow_view()->layer());
}

std::unique_ptr<ui::ScopedLayerAnimationSettings>
ContentsView::CreateTransitionAnimationSettings(ui::Layer* layer) const {
  DCHECK(pagination_model_.has_transition());
  auto settings =
      std::make_unique<ui::ScopedLayerAnimationSettings>(layer->GetAnimator());
  settings->SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
  settings->SetTransitionDuration(
      pagination_model_.GetTransitionAnimationSlideDuration());
  return settings;
}

bool ContentsView::ShouldLayoutPage(AppListPage* page,
                                    AppListState current_state,
                                    AppListState target_state) const {
  if (page == apps_container_view_ || page == search_result_page_view_) {
    return ((current_state == AppListState::kStateSearchResults &&
             target_state == AppListState::kStateApps) ||
            (current_state == AppListState::kStateApps &&
             target_state == AppListState::kStateSearchResults));
  }

  if (page == assistant_page_view_) {
    return current_state == AppListState::kStateEmbeddedAssistant ||
           target_state == AppListState::kStateEmbeddedAssistant;
  }

  return false;
}

gfx::Rect ContentsView::ConvertRectToWidgetWithoutTransform(
    const gfx::Rect& rect) {
  gfx::Rect widget_rect = rect;
  for (const views::View* v = this; v; v = v->parent()) {
    widget_rect.Offset(v->GetMirroredPosition().OffsetFromOrigin());
  }
  return widget_rect;
}

int ContentsView::GetSearchBoxTopForViewState(
    AppListState state,
    AppListViewState view_state) const {
  switch (view_state) {
    case AppListViewState::kClosed:
      return 0;
    case AppListViewState::kFullscreenAllApps:
    case AppListViewState::kFullscreenSearch:
      return apps_container_view_
          ->CalculateMarginsForAvailableBounds(
              GetContentsBounds(), GetSearchBoxSize(AppListState::kStateApps))
          .top();
    case AppListViewState::kPeeking:
    case AppListViewState::kHalf:
      return GetPeekingSearchBoxTopMarginOnPage(state);
  }
}

}  // namespace ash
