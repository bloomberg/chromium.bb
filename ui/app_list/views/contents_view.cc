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
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/apps_container_view.h"
#include "ui/app_list/views/apps_grid_view.h"
#include "ui/app_list/views/search_result_list_view.h"
#include "ui/events/event.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/view_model.h"
#include "ui/views/view_model_utils.h"

namespace app_list {

namespace {

// Indexes of interesting views in ViewModel of ContentsView.
const int kIndexAppsContainer = 0;
const int kIndexSearchResults = 1;

const int kMinMouseWheelToSwitchPage = 20;
const int kMinScrollToSwitchPage = 20;
const int kMinHorizVelocityToSwitchPage = 800;

const double kFinishTransitionThreshold = 0.33;

AppsContainerView* GetAppsContainerView(views::ViewModel* model) {
  return static_cast<AppsContainerView*>(model->view_at(kIndexAppsContainer));
}

SearchResultListView* GetSearchResultListView(views::ViewModel* model) {
  return static_cast<SearchResultListView*>(
      model->view_at(kIndexSearchResults));
}

}  // namespace

ContentsView::ContentsView(AppListMainView* app_list_main_view,
                           PaginationModel* pagination_model,
                           AppListModel* model,
                           AppListViewDelegate* view_delegate)
    : show_state_(SHOW_APPS),
      pagination_model_(pagination_model),
      view_model_(new views::ViewModel),
      bounds_animator_(new views::BoundsAnimator(this)) {
  DCHECK(model);
  pagination_model_->SetTransitionDurations(
      kPageTransitionDurationInMs,
      kOverscrollPageTransitionDurationMs);

  apps_container_view_ =
      new AppsContainerView(app_list_main_view, pagination_model, model);
  AddChildView(apps_container_view_);
  view_model_->Add(apps_container_view_, kIndexAppsContainer);

  SearchResultListView* search_results_view = new SearchResultListView(
      app_list_main_view, view_delegate);
  AddChildView(search_results_view);
  view_model_->Add(search_results_view, kIndexSearchResults);

  GetSearchResultListView(view_model_.get())->SetResults(model->results());
}

ContentsView::~ContentsView() {
}

void ContentsView::CancelDrag() {
  if (apps_container_view_->apps_grid_view()->has_dragged_view())
    apps_container_view_->apps_grid_view()->EndDrag(true);
}

void ContentsView::SetDragAndDropHostOfCurrentAppList(
    ApplicationDragAndDropHost* drag_and_drop_host) {
  apps_container_view_->SetDragAndDropHostOfCurrentAppList(drag_and_drop_host);
}

void ContentsView::SetShowState(ShowState show_state) {
  if (show_state_ == show_state)
    return;

  show_state_ = show_state;
  ShowStateChanged();
}

void ContentsView::ShowStateChanged() {
  SearchResultListView* results_view =
      GetSearchResultListView(view_model_.get());
  // TODO(xiyuan): Highlight default match instead of the first.
  if (show_state_ == SHOW_SEARCH_RESULTS && results_view->visible())
    results_view->SetSelectedIndex(0);
  results_view->UpdateAutoLaunchState();

  AnimateToIdealBounds();
}

void ContentsView::CalculateIdealBounds() {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty())
    return;

  if (app_list::switches::IsExperimentalAppListEnabled()) {
    int incoming_view_index = 0;
    switch (show_state_) {
      case SHOW_APPS:
        incoming_view_index = kIndexAppsContainer;
        break;
      case SHOW_SEARCH_RESULTS:
        incoming_view_index = kIndexSearchResults;
        break;
      default:
        NOTREACHED();
    }

    gfx::Rect incoming_target(rect);
    gfx::Rect outgoing_target(rect);
    outgoing_target.set_y(-outgoing_target.height());

    for (int i = 0; i < view_model_->view_size(); ++i) {
      view_model_->set_ideal_bounds(i,
                                    i == incoming_view_index ? incoming_target
                                                             : outgoing_target);
    }
    return;
  }

  gfx::Rect container_frame(rect);
  gfx::Rect results_frame(rect);

  // Offsets apps grid and result list based on |show_state_|.
  // SearchResultListView is on top of apps grid. Visible view is left in
  // visible area and invisible ones is put out of the visible area.
  int contents_area_height = rect.height();
  switch (show_state_) {
    case SHOW_APPS:
      results_frame.Offset(0, -contents_area_height);
      break;
    case SHOW_SEARCH_RESULTS:
      container_frame.Offset(0, contents_area_height);
      break;
    default:
      NOTREACHED() << "Unknown show_state_ " << show_state_;
      break;
  }

  view_model_->set_ideal_bounds(kIndexAppsContainer, container_frame);
  view_model_->set_ideal_bounds(kIndexSearchResults, results_frame);
}

void ContentsView::AnimateToIdealBounds() {
  CalculateIdealBounds();
  for (int i = 0; i < view_model_->view_size(); ++i) {
    bounds_animator_->AnimateViewTo(view_model_->view_at(i),
                                    view_model_->ideal_bounds(i));
  }
}

void ContentsView::ShowSearchResults(bool show) {
  SetShowState(show ? SHOW_SEARCH_RESULTS : SHOW_APPS);
}

void ContentsView::ShowFolderContent(AppListFolderItem* item) {
  apps_container_view_->ShowActiveFolder(item);
}

void ContentsView::Prerender() {
  const int selected_page = std::max(0, pagination_model_->selected_page());
  apps_container_view_->apps_grid_view()->Prerender(selected_page);
}

gfx::Size ContentsView::GetPreferredSize() {
  const gfx::Size container_size = GetAppsContainerView(view_model_.get())->
      apps_grid_view()->GetPreferredSize();
  const gfx::Size results_size =
      GetSearchResultListView(view_model_.get())->GetPreferredSize();

  int width = std::max(container_size.width(), results_size.width());
  int height = std::max(container_size.height(), results_size.height());
  return gfx::Size(width, height);
}

void ContentsView::Layout() {
  CalculateIdealBounds();
  views::ViewModelUtils::SetViewBoundsToIdealBounds(*view_model_);
}

bool ContentsView::OnKeyPressed(const ui::KeyEvent& event) {
  switch (show_state_) {
    case SHOW_APPS:
      return GetAppsContainerView(view_model_.get())->OnKeyPressed(event);
    case SHOW_SEARCH_RESULTS:
      return GetSearchResultListView(view_model_.get())->OnKeyPressed(event);
    default:
      NOTREACHED() << "Unknown show state " << show_state_;
  }
  return false;
}

bool ContentsView::OnMouseWheel(const ui::MouseWheelEvent& event) {
  if (show_state_ != SHOW_APPS)
    return false;

  int offset;
  if (abs(event.x_offset()) > abs(event.y_offset()))
    offset = event.x_offset();
  else
    offset = event.y_offset();

  if (abs(offset) > kMinMouseWheelToSwitchPage) {
    if (!pagination_model_->has_transition()) {
      pagination_model_->SelectPageRelative(
          offset > 0 ? -1 : 1, true);
    }
    return true;
  }

  return false;
}

void ContentsView::OnGestureEvent(ui::GestureEvent* event) {
  if (show_state_ != SHOW_APPS)
    return;

  switch (event->type()) {
    case ui::ET_GESTURE_SCROLL_BEGIN:
      pagination_model_->StartScroll();
      event->SetHandled();
      return;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      // event->details.scroll_x() > 0 means moving contents to right. That is,
      // transitioning to previous page.
      pagination_model_->UpdateScroll(
          event->details().scroll_x() / GetContentsBounds().width());
      event->SetHandled();
      return;
    case ui::ET_GESTURE_SCROLL_END:
      pagination_model_->EndScroll(pagination_model_->
          transition().progress < kFinishTransitionThreshold);
      event->SetHandled();
      return;
    case ui::ET_SCROLL_FLING_START: {
      pagination_model_->EndScroll(true);
      if (fabs(event->details().velocity_x()) > kMinHorizVelocityToSwitchPage) {
        pagination_model_->SelectPageRelative(
            event->details().velocity_x() < 0 ? 1 : -1,
            true);
      }
      event->SetHandled();
      return;
    }
    default:
      break;
  }
}

void ContentsView::OnScrollEvent(ui::ScrollEvent* event) {
  if (show_state_ != SHOW_APPS ||
      event->type() == ui::ET_SCROLL_FLING_CANCEL) {
    return;
  }

  float offset;
  if (abs(event->x_offset()) > abs(event->y_offset()))
    offset = event->x_offset();
  else
    offset = event->y_offset();

  if (abs(offset) > kMinScrollToSwitchPage) {
    if (!pagination_model_->has_transition()) {
      pagination_model_->SelectPageRelative(offset > 0 ? -1 : 1,
                                            true);
    }
    event->SetHandled();
    event->StopPropagation();
  }
}

}  // namespace app_list
