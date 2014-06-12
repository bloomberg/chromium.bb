// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/contents_view.h"

#include <algorithm>

#include "base/logging.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/pagination_model.h"
#include "ui/app_list/views/app_list_folder_view.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/apps_container_view.h"
#include "ui/app_list/views/apps_grid_view.h"
#include "ui/app_list/views/search_result_list_view.h"
#include "ui/app_list/views/start_page_view.h"
#include "ui/events/event.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/view_model.h"
#include "ui/views/view_model_utils.h"

namespace app_list {

namespace {

const int kMinMouseWheelToSwitchPage = 20;
const int kMinScrollToSwitchPage = 20;
const int kMinHorizVelocityToSwitchPage = 800;

const double kFinishTransitionThreshold = 0.33;

}  // namespace

ContentsView::ContentsView(AppListMainView* app_list_main_view,
                           AppListModel* model,
                           AppListViewDelegate* view_delegate)
    : search_results_view_(NULL),
      start_page_view_(NULL),
      app_list_main_view_(app_list_main_view),
      view_model_(new views::ViewModel),
      bounds_animator_(new views::BoundsAnimator(this)) {
  DCHECK(model);

  if (app_list::switches::IsExperimentalAppListEnabled()) {
    start_page_view_ = new StartPageView(app_list_main_view, view_delegate);
    AddLauncherPage(start_page_view_, NAMED_PAGE_START);
  } else {
    search_results_view_ =
        new SearchResultListView(app_list_main_view, view_delegate);
    AddLauncherPage(search_results_view_, NAMED_PAGE_SEARCH_RESULTS);
    search_results_view_->SetResults(model->results());
  }

  apps_container_view_ = new AppsContainerView(app_list_main_view, model);
  active_page_ = AddLauncherPage(apps_container_view_, NAMED_PAGE_APPS);

}

ContentsView::~ContentsView() {
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
  if (active_page_ == page_index)
    return;

  active_page_ = page_index;
  ActivePageChanged();
}

bool ContentsView::IsNamedPageActive(NamedPage named_page) const {
  std::map<NamedPage, int>::const_iterator it =
      named_page_to_view_.find(named_page);
  if (it == named_page_to_view_.end())
    return false;
  return it->second == active_page_;
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

void ContentsView::ActivePageChanged() {
  // TODO(xiyuan): Highlight default match instead of the first.
  if (IsNamedPageActive(NAMED_PAGE_SEARCH_RESULTS) &&
      search_results_view_->visible()) {
    search_results_view_->SetSelectedIndex(0);
  }
  if (search_results_view_)
    search_results_view_->UpdateAutoLaunchState();

  if (IsNamedPageActive(NAMED_PAGE_START))
    start_page_view_->Reset();

  // Notify parent AppListMainView of the page change.
  app_list_main_view_->UpdateSearchBoxVisibility();

  AnimateToIdealBounds();
}

void ContentsView::ShowSearchResults(bool show) {
  NamedPage new_named_page = show ? NAMED_PAGE_SEARCH_RESULTS : NAMED_PAGE_APPS;
  if (app_list::switches::IsExperimentalAppListEnabled())
    new_named_page = NAMED_PAGE_START;

  SetActivePage(GetPageIndexForNamedPage(new_named_page));

  if (app_list::switches::IsExperimentalAppListEnabled()) {
    if (show)
      start_page_view_->ShowSearchResults();
    else
      start_page_view_->Reset();
    app_list_main_view_->UpdateSearchBoxVisibility();
  }
}

bool ContentsView::IsShowingSearchResults() const {
  return app_list::switches::IsExperimentalAppListEnabled()
             ? IsNamedPageActive(NAMED_PAGE_START) &&
                   start_page_view_->IsShowingSearchResults()
             : IsNamedPageActive(NAMED_PAGE_SEARCH_RESULTS);
}

void ContentsView::CalculateIdealBounds() {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty())
    return;

  if (app_list::switches::IsExperimentalAppListEnabled()) {
    gfx::Rect incoming_target(rect);
    gfx::Rect outgoing_target(rect);
    outgoing_target.set_x(-outgoing_target.width());

    for (int i = 0; i < view_model_->view_size(); ++i) {
      view_model_->set_ideal_bounds(
          i, i == active_page_ ? incoming_target : outgoing_target);
    }
    return;
  }

  gfx::Rect container_frame(rect);
  gfx::Rect results_frame(rect);

  // Offsets apps grid and result list based on |active_page_|.
  // SearchResultListView is on top of apps grid. Visible view is left in
  // visible area and invisible ones is put out of the visible area.
  int contents_area_height = rect.height();
  if (IsNamedPageActive(NAMED_PAGE_APPS))
    results_frame.Offset(0, -contents_area_height);
  else if (IsNamedPageActive(NAMED_PAGE_SEARCH_RESULTS))
    container_frame.Offset(0, contents_area_height);
  else
    NOTREACHED() << "Page " << active_page_ << " invalid in current app list.";

  view_model_->set_ideal_bounds(GetPageIndexForNamedPage(NAMED_PAGE_APPS),
                                container_frame);
  view_model_->set_ideal_bounds(
      GetPageIndexForNamedPage(NAMED_PAGE_SEARCH_RESULTS), results_frame);
}

void ContentsView::AnimateToIdealBounds() {
  CalculateIdealBounds();
  for (int i = 0; i < view_model_->view_size(); ++i) {
    bounds_animator_->AnimateViewTo(view_model_->view_at(i),
                                    view_model_->ideal_bounds(i));
  }
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

int ContentsView::AddLauncherPage(views::View* view) {
  int page_index = view_model_->view_size();
  AddChildView(view);
  view_model_->Add(view, page_index);
  return page_index;
}

int ContentsView::AddLauncherPage(views::View* view, NamedPage named_page) {
  int page_index = AddLauncherPage(view);
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
  CalculateIdealBounds();
  views::ViewModelUtils::SetViewBoundsToIdealBounds(*view_model_);
}

bool ContentsView::OnKeyPressed(const ui::KeyEvent& event) {
  return view_model_->view_at(active_page_)->OnKeyPressed(event);
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
