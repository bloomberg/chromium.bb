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
#include "ui/app_list/views/contents_switcher_view.h"
#include "ui/app_list/views/search_result_list_view.h"
#include "ui/app_list/views/start_page_view.h"
#include "ui/events/event.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/view_model.h"
#include "ui/views/view_model_utils.h"

namespace app_list {

namespace {

const int kMinMouseWheelToSwitchPage = 20;
const int kMinScrollToSwitchPage = 20;
const int kMinHorizVelocityToSwitchPage = 800;

const double kFinishTransitionThreshold = 0.33;

}  // namespace

ContentsView::ContentsView(AppListMainView* app_list_main_view)
    : search_results_view_(NULL),
      start_page_view_(NULL),
      app_list_main_view_(app_list_main_view),
      contents_switcher_view_(NULL),
      view_model_(new views::ViewModel),
      page_before_search_(0) {
  pagination_model_.AddObserver(this);
}

ContentsView::~ContentsView() {
  pagination_model_.RemoveObserver(this);
  if (contents_switcher_view_)
    pagination_model_.RemoveObserver(contents_switcher_view_);
}

void ContentsView::InitNamedPages(AppListModel* model,
                                  AppListViewDelegate* view_delegate) {
  DCHECK(model);

  if (app_list::switches::IsExperimentalAppListEnabled()) {
    std::vector<views::View*> custom_page_views =
        view_delegate->CreateCustomPageWebViews(GetLocalBounds().size());
    for (std::vector<views::View*>::const_iterator it =
             custom_page_views.begin();
         it != custom_page_views.end();
         ++it) {
      AddLauncherPage(*it, IDR_APP_LIST_NOTIFICATIONS_ICON);
    }

    start_page_view_ = new StartPageView(app_list_main_view_, view_delegate);
    AddLauncherPage(
        start_page_view_, IDR_APP_LIST_SEARCH_ICON, NAMED_PAGE_START);
  } else {
    search_results_view_ =
        new SearchResultListView(app_list_main_view_, view_delegate);
    AddLauncherPage(search_results_view_, 0, NAMED_PAGE_SEARCH_RESULTS);
    search_results_view_->SetResults(model->results());
  }

  apps_container_view_ = new AppsContainerView(app_list_main_view_, model);

  int initial_page_index = AddLauncherPage(
      apps_container_view_, IDR_APP_LIST_APPS_ICON, NAMED_PAGE_APPS);
  if (app_list::switches::IsExperimentalAppListEnabled())
    initial_page_index = GetPageIndexForNamedPage(NAMED_PAGE_START);

  page_before_search_ = initial_page_index;
  pagination_model_.SelectPage(initial_page_index, false);

  // Needed to update the main search box visibility.
  ActivePageChanged(false);
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

void ContentsView::SetContentsSwitcherView(
    ContentsSwitcherView* contents_switcher_view) {
  DCHECK(!contents_switcher_view_);
  contents_switcher_view_ = contents_switcher_view;
  if (contents_switcher_view_)
    pagination_model_.AddObserver(contents_switcher_view_);
}

void ContentsView::SetActivePage(int page_index) {
  if (GetActivePageIndex() == page_index)
    return;

  SetActivePageInternal(page_index, false);
}

int ContentsView::GetActivePageIndex() const {
  // The active page is changed at the beginning of an animation, not the end.
  return pagination_model_.SelectedTargetPage();
}

bool ContentsView::IsNamedPageActive(NamedPage named_page) const {
  std::map<NamedPage, int>::const_iterator it =
      named_page_to_view_.find(named_page);
  if (it == named_page_to_view_.end())
    return false;
  return it->second == GetActivePageIndex();
}

int ContentsView::GetPageIndexForNamedPage(NamedPage named_page) const {
  // Find the index of the view corresponding to the given named_page.
  std::map<NamedPage, int>::const_iterator it =
      named_page_to_view_.find(named_page);
  // GetPageIndexForNamedPage should never be called on a named_page that does
  // not have a corresponding view.
  DCHECK(it != named_page_to_view_.end());
  return it->second;
}

int ContentsView::NumLauncherPages() const {
  return pagination_model_.total_pages();
}

void ContentsView::SetActivePageInternal(int page_index,
                                         bool show_search_results) {
  if (!show_search_results)
    page_before_search_ = page_index;
  // Start animating to the new page.
  pagination_model_.SelectPage(page_index, true);
  ActivePageChanged(show_search_results);
}

void ContentsView::ActivePageChanged(bool show_search_results) {
  // TODO(xiyuan): Highlight default match instead of the first.
  if (IsNamedPageActive(NAMED_PAGE_SEARCH_RESULTS) &&
      search_results_view_->visible()) {
    search_results_view_->SetSelectedIndex(0);
  }
  if (search_results_view_)
    search_results_view_->UpdateAutoLaunchState();

  if (IsNamedPageActive(NAMED_PAGE_START)) {
    if (show_search_results)
      start_page_view_->ShowSearchResults();
    else
      start_page_view_->Reset();
  }

  // Notify parent AppListMainView of the page change.
  app_list_main_view_->UpdateSearchBoxVisibility();
}

void ContentsView::ShowSearchResults(bool show) {
  int search_page = GetPageIndexForNamedPage(
      app_list::switches::IsExperimentalAppListEnabled()
          ? NAMED_PAGE_START
          : NAMED_PAGE_SEARCH_RESULTS);

  SetActivePageInternal(show ? search_page : page_before_search_, show);
}

bool ContentsView::IsShowingSearchResults() const {
  return app_list::switches::IsExperimentalAppListEnabled()
             ? IsNamedPageActive(NAMED_PAGE_START) &&
                   start_page_view_->IsShowingSearchResults()
             : IsNamedPageActive(NAMED_PAGE_SEARCH_RESULTS);
}

void ContentsView::UpdatePageBounds() {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty())
    return;

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

  gfx::Rect incoming_target(rect);
  gfx::Rect outgoing_target(rect);
  int dir = target_page > current_page ? -1 : 1;

  if (app_list::switches::IsExperimentalAppListEnabled()) {
    // The experimental app list transitions horizontally.
    int page_width = rect.width();
    int transition_offset = progress * page_width * dir;

    outgoing_target.set_x(transition_offset);
    incoming_target.set_x(dir < 0 ? transition_offset + page_width
                                  : transition_offset - page_width);
  } else {
    // The normal app list transitions vertically.
    int page_height = rect.height();
    int transition_offset = progress * page_height * dir;

    outgoing_target.set_y(transition_offset);
    incoming_target.set_y(dir < 0 ? transition_offset + page_height
                                  : transition_offset - page_height);
  }

  view_model_->view_at(current_page)->SetBoundsRect(outgoing_target);
  view_model_->view_at(target_page)->SetBoundsRect(incoming_target);
}

PaginationModel* ContentsView::GetAppsPaginationModel() {
  return apps_container_view_->apps_grid_view()->pagination_model();
}

void ContentsView::ShowFolderContent(AppListFolderItem* item) {
  apps_container_view_->ShowActiveFolder(item);
}

void ContentsView::Prerender() {
  const int selected_page =
      std::max(0, GetAppsPaginationModel()->selected_page());
  apps_container_view_->apps_grid_view()->Prerender(selected_page);
}

views::View* ContentsView::GetPageView(int index) {
  return view_model_->view_at(index);
}

void ContentsView::AddBlankPageForTesting() {
  AddLauncherPage(new views::View, 0);
}

int ContentsView::AddLauncherPage(views::View* view, int resource_id) {
  int page_index = view_model_->view_size();
  AddChildView(view);
  view_model_->Add(view, page_index);
  if (contents_switcher_view_)
    contents_switcher_view_->AddSwitcherButton(resource_id, page_index);
  pagination_model_.SetTotalPages(view_model_->view_size());
  return page_index;
}

int ContentsView::AddLauncherPage(views::View* view,
                                  int resource_id,
                                  NamedPage named_page) {
  int page_index = AddLauncherPage(view, resource_id);
  named_page_to_view_.insert(std::pair<NamedPage, int>(named_page, page_index));
  return page_index;
}

gfx::Size ContentsView::GetPreferredSize() const {
  const gfx::Size container_size =
      apps_container_view_->apps_grid_view()->GetPreferredSize();
  const gfx::Size results_size = search_results_view_
                                     ? search_results_view_->GetPreferredSize()
                                     : gfx::Size();

  int width = std::max(container_size.width(), results_size.width());
  int height = std::max(container_size.height(), results_size.height());
  return gfx::Size(width, height);
}

void ContentsView::Layout() {
  // Immediately finish all current animations.
  pagination_model_.FinishAnimation();

  // Move the current view onto the screen, and all other views off screen to
  // the left. (Since we are not animating, we don't need to be careful about
  // which side we place the off-screen views onto.)
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty())
    return;

  gfx::Rect offscreen_target(rect);
  offscreen_target.set_x(-rect.width());

  for (int i = 0; i < view_model_->view_size(); ++i) {
    view_model_->view_at(i)->SetBoundsRect(
        i == pagination_model_.SelectedTargetPage() ? rect : offscreen_target);
  }
}

bool ContentsView::OnKeyPressed(const ui::KeyEvent& event) {
  return view_model_->view_at(GetActivePageIndex())->OnKeyPressed(event);
}

bool ContentsView::OnMouseWheel(const ui::MouseWheelEvent& event) {
  if (!IsNamedPageActive(NAMED_PAGE_APPS))
    return false;

  int offset;
  if (abs(event.x_offset()) > abs(event.y_offset()))
    offset = event.x_offset();
  else
    offset = event.y_offset();

  if (abs(offset) > kMinMouseWheelToSwitchPage) {
    if (!GetAppsPaginationModel()->has_transition()) {
      GetAppsPaginationModel()->SelectPageRelative(offset > 0 ? -1 : 1, true);
    }
    return true;
  }

  return false;
}

void ContentsView::TotalPagesChanged() {
}

void ContentsView::SelectedPageChanged(int old_selected, int new_selected) {
}

void ContentsView::TransitionStarted() {
}

void ContentsView::TransitionChanged() {
  UpdatePageBounds();
}

void ContentsView::OnGestureEvent(ui::GestureEvent* event) {
  if (!IsNamedPageActive(NAMED_PAGE_APPS))
    return;

  switch (event->type()) {
    case ui::ET_GESTURE_SCROLL_BEGIN:
      GetAppsPaginationModel()->StartScroll();
      event->SetHandled();
      return;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      // event->details.scroll_x() > 0 means moving contents to right. That is,
      // transitioning to previous page.
      GetAppsPaginationModel()->UpdateScroll(event->details().scroll_x() /
                                             GetContentsBounds().width());
      event->SetHandled();
      return;
    case ui::ET_GESTURE_SCROLL_END:
      GetAppsPaginationModel()->EndScroll(
          GetAppsPaginationModel()->transition().progress <
          kFinishTransitionThreshold);
      event->SetHandled();
      return;
    case ui::ET_SCROLL_FLING_START: {
      GetAppsPaginationModel()->EndScroll(true);
      if (fabs(event->details().velocity_x()) > kMinHorizVelocityToSwitchPage) {
        GetAppsPaginationModel()->SelectPageRelative(
            event->details().velocity_x() < 0 ? 1 : -1, true);
      }
      event->SetHandled();
      return;
    }
    default:
      break;
  }
}

void ContentsView::OnScrollEvent(ui::ScrollEvent* event) {
  if (!IsNamedPageActive(NAMED_PAGE_APPS) ||
      event->type() == ui::ET_SCROLL_FLING_CANCEL) {
    return;
  }

  float offset;
  if (std::abs(event->x_offset()) > std::abs(event->y_offset()))
    offset = event->x_offset();
  else
    offset = event->y_offset();

  if (std::abs(offset) > kMinScrollToSwitchPage) {
    if (!GetAppsPaginationModel()->has_transition()) {
      GetAppsPaginationModel()->SelectPageRelative(offset > 0 ? -1 : 1, true);
    }
    event->SetHandled();
    event->StopPropagation();
  }
}

}  // namespace app_list
