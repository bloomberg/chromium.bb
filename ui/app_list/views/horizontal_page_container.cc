// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/horizontal_page_container.h"

#include "base/strings/utf_string_conversions.h"
#include "chromeos/chromeos_switches.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/pagination_controller.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/app_list/views/apps_container_view.h"
#include "ui/app_list/views/assistant_container_view.h"
#include "ui/app_list/views/contents_view.h"
#include "ui/chromeos/search_box/search_box_constants.h"
#include "ui/views/controls/label.h"

namespace app_list {

// Initial search box top padding in shelf mode.
constexpr int kSearchBoxInitalTopPadding = 12;

// Top padding of search box in peeking state.
constexpr int kSearchBoxPeekingTopPadding = 24;

// Minimum top padding of search box in fullscreen state.
constexpr int kSearchBoxMinimumTopPadding = 24;

HorizontalPageContainer::HorizontalPageContainer(ContentsView* contents_view,
                                                 AppListModel* model)
    : contents_view_(contents_view) {
  pagination_model_.SetTransitionDurations(kPageTransitionDurationInMs,
                                           kOverscrollPageTransitionDurationMs);
  pagination_model_.AddObserver(this);
  pagination_controller_.reset(new PaginationController(
      &pagination_model_, PaginationController::SCROLL_AXIS_HORIZONTAL));

  // Add horizontal pages.
  if (chromeos::switches::IsAssistantEnabled()) {
    assistant_container_view_ = new AssistantContainerView(contents_view_);
    AddHorizontalPage(assistant_container_view_);
  }

  apps_container_view_ = new AppsContainerView(contents_view_, model);
  AddHorizontalPage(apps_container_view_);
  pagination_model_.SetTotalPages(horizontal_pages_.size());

  // By default select apps container page.
  pagination_model_.SelectPage(GetIndexForPage(apps_container_view_), false);
}

HorizontalPageContainer::~HorizontalPageContainer() {
  pagination_model_.RemoveObserver(this);
}

gfx::Size HorizontalPageContainer::CalculatePreferredSize() const {
  gfx::Size size;
  for (auto* page : horizontal_pages_) {
    size.SetToMax(page->GetPreferredSize());
  }
  return size;
}

void HorizontalPageContainer::Layout() {
  gfx::Rect content_bounds(GetContentsBounds());
  for (size_t i = 0; i < horizontal_pages_.size(); ++i) {
    gfx::Rect page_bounds(content_bounds);
    page_bounds.ClampToCenteredSize(horizontal_pages_[i]->GetPreferredSize());
    page_bounds.Offset(GetOffsetForPageIndex(i));
    horizontal_pages_[i]->SetBoundsRect(page_bounds);
  }
}

void HorizontalPageContainer::OnGestureEvent(ui::GestureEvent* event) {
  if (pagination_controller_->OnGestureEvent(*event, GetContentsBounds()))
    event->SetHandled();
}

void HorizontalPageContainer::OnWillBeHidden() {
  GetSelectedPage()->OnWillBeHidden();
}

gfx::Rect HorizontalPageContainer::GetSearchBoxBounds() const {
  return GetSearchBoxBoundsForState(contents_view_->GetActiveState());
}

gfx::Rect HorizontalPageContainer::GetSearchBoxBoundsForState(
    ash::AppListState state) const {
  gfx::Rect search_box_bounds(contents_view_->GetDefaultSearchBoxBounds());
  bool is_in_drag = false;
  if (contents_view_->app_list_view())
    is_in_drag = contents_view_->app_list_view()->is_in_drag();
  if (is_in_drag) {
    search_box_bounds.set_y(GetSearchBoxTopPaddingDuringDragging());
  } else {
    if (state == ash::AppListState::kStateStart)
      search_box_bounds.set_y(kSearchBoxPeekingTopPadding);
    else
      search_box_bounds.set_y(GetSearchBoxFinalTopPadding());
  }

  return search_box_bounds;
}

gfx::Rect HorizontalPageContainer::GetPageBoundsForState(
    ash::AppListState state) const {
  gfx::Rect onscreen_bounds = GetDefaultContentsBounds();

  // Both STATE_START and STATE_APPS are AppsContainerView page.
  if (state == ash::AppListState::kStateApps ||
      state == ash::AppListState::kStateStart) {
    int y = GetSearchBoxBoundsForState(state).bottom();
    if (state == ash::AppListState::kStateStart)
      y -= (kSearchBoxBottomPadding - kSearchBoxPeekingBottomPadding);
    onscreen_bounds.set_y(y);
    return onscreen_bounds;
  }

  return GetBelowContentsOffscreenBounds(onscreen_bounds.size());
}

gfx::Rect HorizontalPageContainer::GetPageBoundsDuringDragging(
    ash::AppListState state) const {
  float app_list_y_position_in_screen =
      contents_view_->app_list_view()->app_list_y_position_in_screen();
  float drag_amount =
      std::max(0.f, contents_view_->app_list_view()->GetScreenBottom() -
                        kShelfSize - app_list_y_position_in_screen);

  float y = 0;
  float peeking_final_y =
      kSearchBoxPeekingTopPadding + search_box::kSearchBoxPreferredHeight +
      kSearchBoxPeekingBottomPadding - kSearchBoxBottomPadding;
  if (drag_amount <= (kPeekingAppListHeight - kShelfSize)) {
    // App list is dragged from collapsed to peeking, which moved up at most
    // |kPeekingAppListHeight - kShelfSize| (272px). The top padding of apps
    // container view changes from |-kSearchBoxFullscreenBottomPadding| to
    // |kSearchBoxPeekingTopPadding + kSearchBoxPreferredHeight +
    // kSearchBoxPeekingBottomPadding - kSearchBoxFullscreenBottomPadding|.
    y = std::ceil(((peeking_final_y + kSearchBoxBottomPadding) * drag_amount) /
                      (kPeekingAppListHeight - kShelfSize) -
                  kSearchBoxBottomPadding);
  } else {
    // App list is dragged from peeking to fullscreen, which moved up at most
    // |peeking_to_fullscreen_height|. The top padding of apps container view
    // changes from |peeking_final_y| to |final_y|.
    float final_y =
        GetSearchBoxFinalTopPadding() + search_box::kSearchBoxPreferredHeight;
    float peeking_to_fullscreen_height =
        contents_view_->GetDisplayHeight() - kPeekingAppListHeight;
    y = std::ceil((final_y - peeking_final_y) *
                      (drag_amount - (kPeekingAppListHeight - kShelfSize)) /
                      peeking_to_fullscreen_height +
                  peeking_final_y);
    y = std::max(std::min(final_y, y), peeking_final_y);
  }

  gfx::Rect onscreen_bounds = GetPageBoundsForState(state);
  // Both STATE_START and STATE_APPS are AppsContainerView page.
  if (state == ash::AppListState::kStateApps ||
      state == ash::AppListState::kStateStart)
    onscreen_bounds.set_y(y);

  return onscreen_bounds;
}

views::View* HorizontalPageContainer::GetFirstFocusableView() {
  return GetSelectedPage()->GetFirstFocusableView();
}

views::View* HorizontalPageContainer::GetLastFocusableView() {
  return GetSelectedPage()->GetLastFocusableView();
}

void HorizontalPageContainer::TotalPagesChanged() {}

void HorizontalPageContainer::SelectedPageChanged(int old_selected,
                                                  int new_selected) {
  Layout();
}

void HorizontalPageContainer::TransitionStarted() {
  Layout();
}

void HorizontalPageContainer::TransitionChanged() {
  Layout();
}

void HorizontalPageContainer::TransitionEnded() {
  Layout();
}

int HorizontalPageContainer::AddHorizontalPage(HorizontalPage* view) {
  AddChildView(view);
  horizontal_pages_.emplace_back(view);
  return horizontal_pages_.size() - 1;
}

int HorizontalPageContainer::GetIndexForPage(HorizontalPage* view) const {
  for (size_t i = 0; i < horizontal_pages_.size(); ++i) {
    if (horizontal_pages_[i] == view)
      return i;
  }
  return -1;
}

HorizontalPage* HorizontalPageContainer::GetSelectedPage() {
  const int current_page = pagination_model_.selected_page();
  DCHECK(pagination_model_.is_valid_page(current_page));
  return horizontal_pages_[current_page];
}

gfx::Vector2d HorizontalPageContainer::GetOffsetForPageIndex(int index) const {
  const int current_page = pagination_model_.selected_page();
  DCHECK(pagination_model_.is_valid_page(current_page));
  const PaginationModel::Transition& transition =
      pagination_model_.transition();
  const bool is_valid = pagination_model_.is_valid_page(transition.target_page);
  const int dir = transition.target_page > current_page ? -1 : 1;
  int x_offset = 0;
  // TODO(https://crbug.com/820510): figure out the right pagination style.
  if (index < current_page)
    x_offset = -width();
  else if (index > current_page)
    x_offset = width();

  if (is_valid) {
    if (index == current_page || index == transition.target_page) {
      x_offset += transition.progress * width() * dir;
    }
  }
  return gfx::Vector2d(x_offset, 0);
}

int HorizontalPageContainer::GetSearchBoxFinalTopPadding() const {
  gfx::Rect search_box_bounds(contents_view_->GetDefaultSearchBoxBounds());
  const int total_height =
      GetDefaultContentsBounds().bottom() - search_box_bounds.y();

  // Makes search box and content vertically centered in contents_view.
  int y = std::max(search_box_bounds.y(),
                   (contents_view_->GetDisplayHeight() - total_height) / 2);

  // Top padding of the searchbox should not be smaller than
  // |kSearchBoxMinimumTopPadding|
  return std::max(y, kSearchBoxMinimumTopPadding);
}

int HorizontalPageContainer::GetSearchBoxTopPaddingDuringDragging() const {
  float searchbox_final_y = GetSearchBoxFinalTopPadding();
  float peeking_to_fullscreen_height =
      contents_view_->GetDisplayHeight() - kPeekingAppListHeight;
  float drag_amount = std::max(
      0, contents_view_->app_list_view()->GetScreenBottom() - kShelfSize -
             contents_view_->app_list_view()->app_list_y_position_in_screen());

  if (drag_amount <= (kPeekingAppListHeight - kShelfSize)) {
    // App list is dragged from collapsed to peeking, which moved up at most
    // |kPeekingAppListHeight - kShelfSize| (272px). The top padding of search
    // box changes from |kSearchBoxInitalTopPadding| to
    // |kSearchBoxPeekingTopPadding|,
    return std::ceil(
        (kSearchBoxPeekingTopPadding - kSearchBoxInitalTopPadding) +
        ((kSearchBoxPeekingTopPadding - kSearchBoxInitalTopPadding) *
         drag_amount) /
            (kPeekingAppListHeight - kShelfSize));
  } else {
    // App list is dragged from peeking to fullscreen, which moved up at most
    // |peeking_to_fullscreen_height|. The top padding of search box changes
    // from |kSearchBoxPeekingTopPadding| to |searchbox_final_y|.
    int y = (kSearchBoxPeekingTopPadding +
             std::ceil((searchbox_final_y - kSearchBoxPeekingTopPadding) *
                       (drag_amount - (kPeekingAppListHeight - kShelfSize)) /
                       peeking_to_fullscreen_height));
    y = std::max(kSearchBoxPeekingTopPadding,
                 std::min<int>(searchbox_final_y, y));
    return y;
  }
}

}  // namespace app_list
