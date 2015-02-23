// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/contents_view.h"

#include <algorithm>
#include <vector>

#include "base/logging.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/views/app_list_folder_view.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/apps_container_view.h"
#include "ui/app_list/views/apps_grid_view.h"
#include "ui/app_list/views/contents_animator.h"
#include "ui/app_list/views/search_box_view.h"
#include "ui/app_list/views/search_result_list_view.h"
#include "ui/app_list/views/search_result_page_view.h"
#include "ui/app_list/views/search_result_tile_item_list_view.h"
#include "ui/app_list/views/start_page_view.h"
#include "ui/events/event.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/view_model.h"
#include "ui/views/view_model_utils.h"
#include "ui/views/widget/widget.h"

namespace app_list {

ContentsView::ContentsView(AppListMainView* app_list_main_view)
    : apps_container_view_(nullptr),
      search_results_page_view_(nullptr),
      start_page_view_(nullptr),
      custom_page_view_(nullptr),
      app_list_main_view_(app_list_main_view),
      view_model_(new views::ViewModel),
      page_before_search_(0) {
  pagination_model_.SetTransitionDurations(kPageTransitionDurationInMs,
                                           kOverscrollPageTransitionDurationMs);
  pagination_model_.AddObserver(this);
}

ContentsView::~ContentsView() {
  pagination_model_.RemoveObserver(this);
}

void ContentsView::Init(AppListModel* model) {
  DCHECK(model);

  AppListViewDelegate* view_delegate = app_list_main_view_->view_delegate();

  if (app_list::switches::IsExperimentalAppListEnabled()) {
    std::vector<views::View*> custom_page_views =
        view_delegate->CreateCustomPageWebViews(GetLocalBounds().size());
    for (std::vector<views::View*>::const_iterator it =
             custom_page_views.begin();
         it != custom_page_views.end();
         ++it) {
      // Only the first launcher page is considered to represent
      // STATE_CUSTOM_LAUNCHER_PAGE.
      if (it == custom_page_views.begin()) {
        custom_page_view_ = *it;

        AddLauncherPage(*it, AppListModel::STATE_CUSTOM_LAUNCHER_PAGE);
      } else {
        AddLauncherPage(*it);
      }
    }

    // Start page.
    start_page_view_ = new StartPageView(app_list_main_view_, view_delegate);
    AddLauncherPage(start_page_view_, AppListModel::STATE_START);
  }

  // Search results UI.
  search_results_page_view_ = new SearchResultPageView();

  AppListModel::SearchResults* results = view_delegate->GetModel()->results();
  search_results_page_view_->AddSearchResultContainerView(
      results, new SearchResultListView(app_list_main_view_, view_delegate));

  if (app_list::switches::IsExperimentalAppListEnabled()) {
    search_results_page_view_->AddSearchResultContainerView(
        results,
        new SearchResultTileItemListView(GetSearchBoxView()->search_box()));
  }
  AddLauncherPage(search_results_page_view_,
                  AppListModel::STATE_SEARCH_RESULTS);

  apps_container_view_ = new AppsContainerView(app_list_main_view_, model);

  AddLauncherPage(apps_container_view_, AppListModel::STATE_APPS);

  int initial_page_index = app_list::switches::IsExperimentalAppListEnabled()
                               ? GetPageIndexForState(AppListModel::STATE_START)
                               : GetPageIndexForState(AppListModel::STATE_APPS);
  DCHECK_GE(initial_page_index, 0);

  page_before_search_ = initial_page_index;
  // Must only call SetTotalPages once all the launcher pages have been added
  // (as it will trigger a SelectedPageChanged call).
  pagination_model_.SetTotalPages(view_model_->view_size());
  pagination_model_.SelectPage(initial_page_index, false);

  ActivePageChanged();

  // Populate the contents animators.
  AddAnimator(AppListModel::STATE_START, AppListModel::STATE_APPS,
              scoped_ptr<ContentsAnimator>(new StartToAppsAnimator(this)));
  AddAnimator(AppListModel::STATE_START,
              AppListModel::STATE_CUSTOM_LAUNCHER_PAGE,
              scoped_ptr<ContentsAnimator>(new StartToCustomAnimator(this)));
  default_animator_.reset(new DefaultAnimator(this));
}

void ContentsView::CancelDrag() {
  if (apps_container_view_->apps_grid_view()->has_dragged_view())
    apps_container_view_->apps_grid_view()->EndDrag(true);
  if (apps_container_view_->app_list_folder_view()
          ->items_grid_view()
          ->has_dragged_view()) {
    apps_container_view_->app_list_folder_view()->items_grid_view()->EndDrag(
        true);
  }
}

void ContentsView::SetDragAndDropHostOfCurrentAppList(
    ApplicationDragAndDropHost* drag_and_drop_host) {
  apps_container_view_->SetDragAndDropHostOfCurrentAppList(drag_and_drop_host);
}

void ContentsView::SetActivePage(int page_index) {
  SetActivePage(page_index, true);
}

void ContentsView::SetActivePage(int page_index, bool animate) {
  if (GetActivePageIndex() == page_index)
    return;

  SetActivePageInternal(page_index, false, animate);
}

int ContentsView::GetActivePageIndex() const {
  // The active page is changed at the beginning of an animation, not the end.
  return pagination_model_.SelectedTargetPage();
}

AppListModel::State ContentsView::GetActiveState() const {
  return GetStateForPageIndex(GetActivePageIndex());
}

bool ContentsView::IsStateActive(AppListModel::State state) const {
  int active_page_index = GetActivePageIndex();
  return active_page_index >= 0 &&
         GetPageIndexForState(state) == active_page_index;
}

int ContentsView::GetPageIndexForState(AppListModel::State state) const {
  // Find the index of the view corresponding to the given state.
  std::map<AppListModel::State, int>::const_iterator it =
      state_to_view_.find(state);
  if (it == state_to_view_.end())
    return -1;

  return it->second;
}

AppListModel::State ContentsView::GetStateForPageIndex(int index) const {
  std::map<int, AppListModel::State>::const_iterator it =
      view_to_state_.find(index);
  if (it == view_to_state_.end())
    return AppListModel::INVALID_STATE;

  return it->second;
}

int ContentsView::NumLauncherPages() const {
  return pagination_model_.total_pages();
}

void ContentsView::SetActivePageInternal(int page_index,
                                         bool show_search_results,
                                         bool animate) {
  if (!GetPageView(page_index)->visible())
    return;

  if (!show_search_results)
    page_before_search_ = page_index;
  // Start animating to the new page.
  pagination_model_.SelectPage(page_index, animate);
  ActivePageChanged();

  if (!animate)
    Layout();
}

void ContentsView::ActivePageChanged() {
  AppListModel::State state = AppListModel::INVALID_STATE;

  // TODO(calamity): This does not report search results being shown in the
  // experimental app list as a boolean is currently used to indicate whether
  // search results are showing. See http://crbug.com/427787/.
  std::map<int, AppListModel::State>::const_iterator it =
      view_to_state_.find(pagination_model_.SelectedTargetPage());
  if (it != view_to_state_.end())
    state = it->second;

  app_list_main_view_->model()->SetState(state);

  if (switches::IsExperimentalAppListEnabled()) {
    DCHECK(start_page_view_);

    // Set the visibility of the search box's back button.
    app_list_main_view_->search_box_view()->back_button()->SetVisible(
        state != AppListModel::STATE_START);
    app_list_main_view_->search_box_view()->Layout();

    // Whenever the page changes, the custom launcher page is considered to have
    // been reset.
    app_list_main_view_->model()->ClearCustomLauncherPageSubpages();
  }

  app_list_main_view_->search_box_view()->ResetTabFocus(false);
  apps_container_view_->apps_grid_view()->ClearAnySelectedView();
  apps_container_view_->app_list_folder_view()->items_grid_view()
      ->ClearAnySelectedView();

  if (custom_page_view_) {
    custom_page_view_->SetFocusable(state ==
                                    AppListModel::STATE_CUSTOM_LAUNCHER_PAGE);
  }
}

void ContentsView::ShowSearchResults(bool show) {
  int search_page = GetPageIndexForState(AppListModel::STATE_SEARCH_RESULTS);
  DCHECK_GE(search_page, 0);

  SetActivePageInternal(show ? search_page : page_before_search_, show, true);
}

bool ContentsView::IsShowingSearchResults() const {
  return IsStateActive(AppListModel::STATE_SEARCH_RESULTS);
}

void ContentsView::NotifyCustomLauncherPageAnimationChanged(double progress,
                                                            int current_page,
                                                            int target_page) {
  int custom_launcher_page_index =
      GetPageIndexForState(AppListModel::STATE_CUSTOM_LAUNCHER_PAGE);
  if (custom_launcher_page_index == target_page) {
    app_list_main_view_->view_delegate()->CustomLauncherPageAnimationChanged(
        progress);
  } else if (custom_launcher_page_index == current_page) {
    app_list_main_view_->view_delegate()->CustomLauncherPageAnimationChanged(
        1 - progress);
  }
}

void ContentsView::UpdatePageBounds() {
  // The bounds calculations will potentially be mid-transition (depending on
  // the state of the PaginationModel).
  int current_page = std::max(0, pagination_model_.selected_page());
  int target_page = current_page;
  double progress = 1;
  if (pagination_model_.has_transition()) {
    const PaginationModel::Transition& transition =
        pagination_model_.transition();
    if (pagination_model_.is_valid_page(transition.target_page)) {
      target_page = transition.target_page;
      progress = transition.progress;
    }
  }

  NotifyCustomLauncherPageAnimationChanged(progress, current_page, target_page);

  bool reverse;
  ContentsAnimator* animator =
      GetAnimatorForTransition(current_page, target_page, &reverse);

  // Animate linearly (the PaginationModel handles easing).
  if (reverse)
    animator->Update(1 - progress, target_page, current_page);
  else
    animator->Update(progress, current_page, target_page);
}

PaginationModel* ContentsView::GetAppsPaginationModel() {
  return apps_container_view_->apps_grid_view()->pagination_model();
}

void ContentsView::AddAnimator(AppListModel::State from_state,
                               AppListModel::State to_state,
                               scoped_ptr<ContentsAnimator> animator) {
  int from_page = GetPageIndexForState(from_state);
  int to_page = GetPageIndexForState(to_state);
  contents_animators_.insert(
      std::make_pair(std::make_pair(from_page, to_page),
                     linked_ptr<ContentsAnimator>(animator.release())));
}

ContentsAnimator* ContentsView::GetAnimatorForTransition(int from_page,
                                                         int to_page,
                                                         bool* reverse) const {
  auto it = contents_animators_.find(std::make_pair(from_page, to_page));
  if (it != contents_animators_.end()) {
    *reverse = false;
    return it->second.get();
  }

  it = contents_animators_.find(std::make_pair(to_page, from_page));
  if (it != contents_animators_.end()) {
    *reverse = true;
    return it->second.get();
  }

  *reverse = false;
  return default_animator_.get();
}

void ContentsView::ShowFolderContent(AppListFolderItem* item) {
  apps_container_view_->ShowActiveFolder(item);
}

void ContentsView::Prerender() {
  apps_container_view_->apps_grid_view()->Prerender();
}

views::View* ContentsView::GetPageView(int index) const {
  return view_model_->view_at(index);
}

SearchBoxView* ContentsView::GetSearchBoxView() const {
  return app_list_main_view_->search_box_view();
}

void ContentsView::AddBlankPageForTesting() {
  AddLauncherPage(new views::View);
  pagination_model_.SetTotalPages(view_model_->view_size());
}

int ContentsView::AddLauncherPage(views::View* view) {
  int page_index = view_model_->view_size();
  AddChildView(view);
  view_model_->Add(view, page_index);
  return page_index;
}

int ContentsView::AddLauncherPage(views::View* view,
                                  AppListModel::State state) {
  int page_index = AddLauncherPage(view);
  bool success =
      state_to_view_.insert(std::make_pair(state, page_index)).second;
  success = success &&
            view_to_state_.insert(std::make_pair(page_index, state)).second;

  // There shouldn't be duplicates in either map.
  DCHECK(success);
  return page_index;
}

gfx::Rect ContentsView::GetOnscreenPageBounds(int page_index) const {
  AppListModel::State state = GetStateForPageIndex(page_index);
  bool fills_contents_view =
      state == AppListModel::STATE_CUSTOM_LAUNCHER_PAGE ||
      state == AppListModel::STATE_START;
  return fills_contents_view ? GetContentsBounds() : GetDefaultContentsBounds();
}

gfx::Rect ContentsView::GetOffscreenPageBounds(int page_index) const {
  AppListModel::State state = GetStateForPageIndex(page_index);
  gfx::Rect bounds(GetOnscreenPageBounds(page_index));
  // The start page and search page origins are above; all other pages' origins
  // are below.
  bool origin_above = state == AppListModel::STATE_START ||
                      state == AppListModel::STATE_SEARCH_RESULTS;
  bounds.set_y(origin_above ? -bounds.height()
                            : GetContentsBounds().height() + bounds.y());
  return bounds;
}

gfx::Rect ContentsView::GetDefaultSearchBoxBounds() const {
  gfx::Rect search_box_bounds(0, 0, GetDefaultContentsSize().width(),
                              GetSearchBoxView()->GetPreferredSize().height());
  if (switches::IsExperimentalAppListEnabled()) {
    search_box_bounds.set_y(kExperimentalSearchBoxPadding);
    search_box_bounds.Inset(kExperimentalSearchBoxPadding, 0);
  }
  return search_box_bounds;
}

gfx::Rect ContentsView::GetSearchBoxBoundsForState(
    AppListModel::State state) const {
  // On the start page, the search box is in a different location.
  if (state == AppListModel::STATE_START) {
    DCHECK(start_page_view_);
    // Convert to ContentsView space, assuming that the StartPageView is in the
    // ContentsView's default bounds.
    return start_page_view_->GetSearchBoxBounds() +
           GetOnscreenPageBounds(GetPageIndexForState(state))
               .OffsetFromOrigin();
  }

  return GetDefaultSearchBoxBounds();
}

gfx::Rect ContentsView::GetSearchBoxBoundsForPageIndex(int index) const {
  return GetSearchBoxBoundsForState(GetStateForPageIndex(index));
}

gfx::Rect ContentsView::GetDefaultContentsBounds() const {
  gfx::Rect bounds(gfx::Point(0, GetDefaultSearchBoxBounds().bottom()),
                   GetDefaultContentsSize());
  return bounds;
}

gfx::Rect ContentsView::GetCustomPageCollapsedBounds() const {
  gfx::Rect bounds(GetContentsBounds());
  int page_height = bounds.height();
  bounds.set_y(page_height - kCustomPageCollapsedHeight);
  return bounds;
}

bool ContentsView::Back() {
  AppListModel::State state = view_to_state_[GetActivePageIndex()];
  switch (state) {
    case AppListModel::STATE_START:
      // Close the app list when Back() is called from the start page.
      return false;
    case AppListModel::STATE_CUSTOM_LAUNCHER_PAGE:
      if (app_list_main_view_->model()->PopCustomLauncherPageSubpage())
        app_list_main_view_->view_delegate()->CustomLauncherPagePopSubpage();
      else
        SetActivePage(GetPageIndexForState(AppListModel::STATE_START));
      break;
    case AppListModel::STATE_APPS:
      if (apps_container_view_->IsInFolderView())
        apps_container_view_->app_list_folder_view()->CloseFolderPage();
      else
        SetActivePage(GetPageIndexForState(AppListModel::STATE_START));
      break;
    case AppListModel::STATE_SEARCH_RESULTS:
      GetSearchBoxView()->ClearSearch();
      ShowSearchResults(false);
      break;
    case AppListModel::INVALID_STATE:  // Falls through.
      NOTREACHED();
      break;
  }
  return true;
}

gfx::Size ContentsView::GetDefaultContentsSize() const {
  return apps_container_view_->apps_grid_view()->GetPreferredSize();
}

gfx::Size ContentsView::GetPreferredSize() const {
  gfx::Rect search_box_bounds = GetDefaultSearchBoxBounds();
  gfx::Rect default_contents_bounds = GetDefaultContentsBounds();
  gfx::Vector2d bottom_right =
      search_box_bounds.bottom_right().OffsetFromOrigin();
  bottom_right.SetToMax(
      default_contents_bounds.bottom_right().OffsetFromOrigin());
  return gfx::Size(bottom_right.x(), bottom_right.y());
}

void ContentsView::Layout() {
  // Immediately finish all current animations.
  pagination_model_.FinishAnimation();

  // Move the current view onto the screen, and all other views off screen to
  // the left. (Since we are not animating, we don't need to be careful about
  // which side we place the off-screen views onto.)
  gfx::Rect rect = GetOnscreenPageBounds(GetActivePageIndex());
  double progress =
      IsStateActive(AppListModel::STATE_CUSTOM_LAUNCHER_PAGE) ? 1 : 0;

  // Notify the custom launcher page that the active page has changed.
  app_list_main_view_->view_delegate()->CustomLauncherPageAnimationChanged(
      progress);

  if (rect.IsEmpty())
    return;

  gfx::Rect offscreen_target(rect);
  offscreen_target.set_x(-rect.width());

  int current_page = GetActivePageIndex();

  for (int i = 0; i < view_model_->view_size(); ++i) {
    view_model_->view_at(i)
        ->SetBoundsRect(i == current_page ? rect : offscreen_target);
  }

  // Custom locations of pages in certain states.
  // Within the start page, the custom page is given its collapsed bounds.
  int start_page_index = GetPageIndexForState(AppListModel::STATE_START);
  if (current_page == start_page_index) {
    if (custom_page_view_)
      custom_page_view_->SetBoundsRect(GetCustomPageCollapsedBounds());
  }

  // The search box is contained in a widget so set the bounds of the widget
  // rather than the SearchBoxView. In athena, the search box widget will be the
  // same as the app list widget so don't move it.
  views::Widget* search_box_widget = GetSearchBoxView()->GetWidget();
  if (search_box_widget && search_box_widget != GetWidget()) {
    gfx::Rect search_box_bounds = GetSearchBoxBoundsForState(GetActiveState());
    search_box_widget->SetBounds(ConvertRectToWidget(
        GetSearchBoxView()->GetViewBoundsForSearchBoxContentsBounds(
            search_box_bounds)));
  }
}

bool ContentsView::OnKeyPressed(const ui::KeyEvent& event) {
  bool handled =
      view_model_->view_at(GetActivePageIndex())->OnKeyPressed(event);

  if (!handled) {
    if (event.key_code() == ui::VKEY_TAB && event.IsShiftDown()) {
      GetSearchBoxView()->MoveTabFocus(true);
      handled = true;
    }
  }

  return handled;
}

void ContentsView::TotalPagesChanged() {
}

void ContentsView::SelectedPageChanged(int old_selected, int new_selected) {
  // TODO(mgiuca): This should be generalized so we call a virtual OnShow and
  // OnHide method for each page.
  if (!start_page_view_)
    return;

  if (new_selected == GetPageIndexForState(AppListModel::STATE_START)) {
    start_page_view_->OnShow();
    // Show or hide the custom page view, based on whether it is enabled.
    if (custom_page_view_) {
      custom_page_view_->SetVisible(
          app_list_main_view_->ShouldShowCustomLauncherPage());
    }
  }
}

void ContentsView::TransitionStarted() {
}

void ContentsView::TransitionChanged() {
  UpdatePageBounds();
}

}  // namespace app_list
