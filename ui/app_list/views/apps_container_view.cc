// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/apps_container_view.h"

#include <algorithm>
#include <vector>

#include "base/command_line.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_features.h"
#include "ui/app_list/app_list_folder_item.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/views/app_list_folder_view.h"
#include "ui/app_list/views/app_list_item_view.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/app_list/views/apps_grid_view.h"
#include "ui/app_list/views/contents_view.h"
#include "ui/app_list/views/folder_background_view.h"
#include "ui/app_list/views/search_box_view.h"
#include "ui/app_list/views/suggestions_container_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/textfield/textfield.h"

namespace app_list {

// Initial search box top padding in shelf mode.
constexpr int kSearchBoxInitalTopPadding = 12;

// Top padding of search box in peeking state.
constexpr int kSearchBoxPeekingTopPadding = 24;

// Minimum top padding of search box in fullscreen state.
constexpr int kSearchBoxMinimumTopPadding = 24;

AppsContainerView::AppsContainerView(AppListMainView* app_list_main_view,
                                     AppListModel* model)
    : is_fullscreen_app_list_enabled_(features::IsFullscreenAppListEnabled()),
      is_app_list_focus_enabled_(features::IsAppListFocusEnabled()) {
  apps_grid_view_ =
      new AppsGridView(app_list_main_view->contents_view(), nullptr);
  if (is_fullscreen_app_list_enabled_) {
    apps_grid_view_->SetLayout(kPreferredColsFullscreen,
                               kPreferredRowsFullscreen);
  } else {
    apps_grid_view_->SetLayout(kPreferredCols, kPreferredRows);
  }
  AddChildView(apps_grid_view_);

  folder_background_view_ = new FolderBackgroundView();
  AddChildView(folder_background_view_);

  app_list_folder_view_ =
      new AppListFolderView(this, model, app_list_main_view);
  // The folder view is initially hidden.
  app_list_folder_view_->SetVisible(false);
  AddChildView(app_list_folder_view_);

  apps_grid_view_->SetModel(model);
  apps_grid_view_->SetItemList(model->top_level_item_list());
  SetShowState(SHOW_APPS, false);
}

AppsContainerView::~AppsContainerView() = default;

void AppsContainerView::ShowActiveFolder(AppListFolderItem* folder_item) {
  // Prevent new animations from starting if there are currently animations
  // pending. This fixes crbug.com/357099.
  if (top_icon_animation_pending_count_)
    return;

  app_list_folder_view_->SetAppListFolderItem(folder_item);
  SetShowState(SHOW_ACTIVE_FOLDER, false);

  CreateViewsForFolderTopItemsAnimation(folder_item, true);

  if (is_app_list_focus_enabled_)
    contents_view()->GetSearchBoxView()->search_box()->RequestFocus();
  else
    apps_grid_view_->ClearAnySelectedView();
}

void AppsContainerView::ShowApps(AppListFolderItem* folder_item) {
  if (top_icon_animation_pending_count_)
    return;

  PrepareToShowApps(folder_item);
  SetShowState(SHOW_APPS, true);
}

void AppsContainerView::ResetForShowApps() {
  SetShowState(SHOW_APPS, false);
  folder_background_view_->UpdateFolderContainerBubble(
      FolderBackgroundView::NO_BUBBLE);
}

void AppsContainerView::SetDragAndDropHostOfCurrentAppList(
    ApplicationDragAndDropHost* drag_and_drop_host) {
  apps_grid_view()->SetDragAndDropHostOfCurrentAppList(drag_and_drop_host);
  app_list_folder_view()->items_grid_view()->SetDragAndDropHostOfCurrentAppList(
      drag_and_drop_host);
}

void AppsContainerView::ReparentFolderItemTransit(
    AppListFolderItem* folder_item) {
  if (top_icon_animation_pending_count_)
    return;

  PrepareToShowApps(folder_item);
  SetShowState(SHOW_ITEM_REPARENT, false);
}

bool AppsContainerView::IsInFolderView() const {
  return show_state_ == SHOW_ACTIVE_FOLDER;
}

void AppsContainerView::ReparentDragEnded() {
  DCHECK_EQ(SHOW_ITEM_REPARENT, show_state_);
  show_state_ = AppsContainerView::SHOW_APPS;
}

gfx::Size AppsContainerView::CalculatePreferredSize() const {
  const gfx::Size grid_size = apps_grid_view_->GetPreferredSize();
  const gfx::Size folder_view_size = app_list_folder_view_->GetPreferredSize();

  int width = std::max(grid_size.width(), folder_view_size.width());
  int height = std::max(grid_size.height(), folder_view_size.height());
  return gfx::Size(width, height);
}

void AppsContainerView::Layout() {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty())
    return;

  switch (show_state_) {
    case SHOW_APPS:
      apps_grid_view_->SetBoundsRect(rect);
      break;
    case SHOW_ACTIVE_FOLDER:
      folder_background_view_->SetBoundsRect(rect);
      app_list_folder_view_->SetBoundsRect(rect);
      break;
    case SHOW_ITEM_REPARENT:
      break;
    default:
      NOTREACHED();
  }
}

bool AppsContainerView::OnKeyPressed(const ui::KeyEvent& event) {
  if (show_state_ == SHOW_APPS)
    return apps_grid_view_->OnKeyPressed(event);
  else
    return app_list_folder_view_->OnKeyPressed(event);
}

void AppsContainerView::OnWillBeShown() {
  apps_grid_view()->ClearAnySelectedView();
  app_list_folder_view()->items_grid_view()->ClearAnySelectedView();
}

gfx::Rect AppsContainerView::GetSearchBoxBounds() const {
  return GetSearchBoxBoundsForState(contents_view()->GetActiveState());
}

gfx::Rect AppsContainerView::GetSearchBoxBoundsForState(
    AppListModel::State state) const {
  if (!is_fullscreen_app_list_enabled_)
    return AppListPage::GetSearchBoxBounds();

  gfx::Rect search_box_bounds(contents_view()->GetDefaultSearchBoxBounds());
  bool is_in_drag = false;
  if (contents_view()->app_list_view())
    is_in_drag = contents_view()->app_list_view()->is_in_drag();
  if (is_in_drag) {
    search_box_bounds.set_y(GetSearchBoxTopPaddingDuringDragging());
  } else {
    if (state == AppListModel::STATE_START)
      search_box_bounds.set_y(kSearchBoxPeekingTopPadding);
    else
      search_box_bounds.set_y(GetSearchBoxFinalTopPadding());
  }

  return search_box_bounds;
}

gfx::Rect AppsContainerView::GetPageBoundsForState(
    AppListModel::State state) const {
  gfx::Rect onscreen_bounds = GetDefaultContentsBounds();

  if (!is_fullscreen_app_list_enabled_) {
    if (state == AppListModel::STATE_APPS)
      return onscreen_bounds;
    return GetBelowContentsOffscreenBounds(onscreen_bounds.size());
  }

  // Both STATE_START and STATE_APPS are AppsContainerView page.
  if (state == AppListModel::STATE_APPS || state == AppListModel::STATE_START) {
    int y = GetSearchBoxBoundsForState(state).bottom();
    if (state == AppListModel::STATE_START)
      y -= (kSearchBoxFullscreenBottomPadding - kSearchBoxPeekingBottomPadding);
    onscreen_bounds.set_y(y);
    return onscreen_bounds;
  }

  return GetBelowContentsOffscreenBounds(onscreen_bounds.size());
}

gfx::Rect AppsContainerView::GetPageBoundsDuringDragging(
    AppListModel::State state) const {
  float app_list_y_position_in_screen =
      contents_view()->app_list_view()->app_list_y_position_in_screen();
  float work_area_bottom =
      contents_view()->app_list_view()->GetWorkAreaBottom();
  float drag_amount =
      std::max(0.f, work_area_bottom - app_list_y_position_in_screen);

  float y = 0;
  float peeking_final_y =
      kSearchBoxPeekingTopPadding + kSearchBoxPreferredHeight +
      kSearchBoxPeekingBottomPadding - kSearchBoxFullscreenBottomPadding;
  if (drag_amount <= (kPeekingAppListHeight - kShelfSize)) {
    // App list is dragged from collapsed to peeking, which moved up at most
    // |kPeekingAppListHeight - kShelfSize| (272px). The top padding of apps
    // container view changes from |-kSearchBoxFullscreenBottomPadding| to
    // |kSearchBoxPeekingTopPadding + kSearchBoxPreferredHeight +
    // kSearchBoxPeekingBottomPadding - kSearchBoxFullscreenBottomPadding|.
    y = std::ceil(
        ((peeking_final_y + kSearchBoxFullscreenBottomPadding) * drag_amount) /
            (kPeekingAppListHeight - kShelfSize) -
        kSearchBoxFullscreenBottomPadding);
  } else {
    // App list is dragged from peeking to fullscreen, which moved up at most
    // |peeking_to_fullscreen_height|. The top padding of apps container view
    // changes from |peeking_final_y| to |final_y|.
    float final_y = GetSearchBoxFinalTopPadding() + kSearchBoxPreferredHeight;
    float peeking_to_fullscreen_height =
        contents_view()->GetDisplayHeight() - kPeekingAppListHeight;
    y = std::ceil((final_y - peeking_final_y) *
                      (drag_amount - (kPeekingAppListHeight - kShelfSize)) /
                      peeking_to_fullscreen_height +
                  peeking_final_y);
    y = std::max(std::min(final_y, y), peeking_final_y);
  }

  gfx::Rect onscreen_bounds = GetPageBoundsForState(state);
  // Both STATE_START and STATE_APPS are AppsContainerView page.
  if (state == AppListModel::STATE_APPS || state == AppListModel::STATE_START)
    onscreen_bounds.set_y(y);

  return onscreen_bounds;
}

views::View* AppsContainerView::GetSelectedView() const {
  return IsInFolderView()
             ? app_list_folder_view_->items_grid_view()->GetSelectedView()
             : apps_grid_view_->GetSelectedView();
}

void AppsContainerView::OnTopIconAnimationsComplete() {
  --top_icon_animation_pending_count_;

  if (!top_icon_animation_pending_count_) {
    // Clean up the transitional views used for top item icon animation.
    top_icon_views_.clear();

    // Show the folder icon when closing the folder.
    if ((show_state_ == SHOW_APPS || show_state_ == SHOW_ITEM_REPARENT) &&
        apps_grid_view_->activated_folder_item_view()) {
      apps_grid_view_->activated_folder_item_view()->SetVisible(true);
    }
  }
}

void AppsContainerView::SetShowState(ShowState show_state,
                                     bool show_apps_with_animation) {
  if (show_state_ == show_state)
    return;

  show_state_ = show_state;

  switch (show_state_) {
    case SHOW_APPS:
      folder_background_view_->SetVisible(false);
      if (show_apps_with_animation) {
        app_list_folder_view_->ScheduleShowHideAnimation(false, false);
        apps_grid_view_->ScheduleShowHideAnimation(true);
      } else {
        app_list_folder_view_->HideViewImmediately();
        apps_grid_view_->ResetForShowApps();
      }
      break;
    case SHOW_ACTIVE_FOLDER:
      folder_background_view_->SetVisible(true);
      apps_grid_view_->ScheduleShowHideAnimation(false);
      app_list_folder_view_->ScheduleShowHideAnimation(true, false);
      break;
    case SHOW_ITEM_REPARENT:
      folder_background_view_->SetVisible(false);
      folder_background_view_->UpdateFolderContainerBubble(
          FolderBackgroundView::NO_BUBBLE);
      app_list_folder_view_->ScheduleShowHideAnimation(false, true);
      apps_grid_view_->ScheduleShowHideAnimation(true);
      break;
    default:
      NOTREACHED();
  }

  app_list_folder_view_->SetBackButtonLabel(IsInFolderView());
  Layout();
}

std::vector<gfx::Rect> AppsContainerView::GetTopItemIconBoundsInActiveFolder() {
  // Get the active folder's icon bounds relative to AppsContainerView.
  AppListItemView* folder_item_view =
      apps_grid_view_->activated_folder_item_view();
  gfx::Rect to_grid_view =
      folder_item_view->ConvertRectToParent(folder_item_view->GetIconBounds());
  gfx::Rect to_container = apps_grid_view_->ConvertRectToParent(to_grid_view);

  return FolderImage::GetTopIconsBounds(to_container);
}

void AppsContainerView::CreateViewsForFolderTopItemsAnimation(
    AppListFolderItem* active_folder,
    bool open_folder) {
  top_icon_views_.clear();
  std::vector<gfx::Rect> top_items_bounds =
      GetTopItemIconBoundsInActiveFolder();
  top_icon_animation_pending_count_ =
      std::min(kNumFolderTopItems, active_folder->item_list()->item_count());
  for (size_t i = 0; i < top_icon_animation_pending_count_; ++i) {
    if (active_folder->GetTopIcon(i).isNull())
      continue;

    TopIconAnimationView* icon_view = new TopIconAnimationView(
        active_folder->GetTopIcon(i), top_items_bounds[i], open_folder);
    icon_view->AddObserver(this);
    top_icon_views_.push_back(icon_view);

    // Add the transitional views into child views, and set its bounds to the
    // same location of the item in the folder list view.
    AddChildView(top_icon_views_[i]);
    top_icon_views_[i]->SetBoundsRect(
        app_list_folder_view_->ConvertRectToParent(
            app_list_folder_view_->GetItemIconBoundsAt(i)));
    static_cast<TopIconAnimationView*>(top_icon_views_[i])->TransformView();
  }
}

void AppsContainerView::PrepareToShowApps(AppListFolderItem* folder_item) {
  if (folder_item)
    CreateViewsForFolderTopItemsAnimation(folder_item, false);

  // Hide the active folder item until the animation completes.
  if (apps_grid_view_->activated_folder_item_view())
    apps_grid_view_->activated_folder_item_view()->SetVisible(false);
}

int AppsContainerView::GetSearchBoxFinalTopPadding() const {
  gfx::Rect search_box_bounds(contents_view()->GetDefaultSearchBoxBounds());
  const int total_height =
      GetDefaultContentsBounds().bottom() - search_box_bounds.y();

  // Makes search box and content vertically centered in contents_view.
  int y = std::max(search_box_bounds.y(),
                   (contents_view()->GetDisplayHeight() - total_height) / 2);

  // Top padding of the searchbox should not be smaller than
  // |kSearchBoxMinimumTopPadding|
  return std::max(y, kSearchBoxMinimumTopPadding);
}

int AppsContainerView::GetSearchBoxTopPaddingDuringDragging() const {
  float searchbox_final_y = GetSearchBoxFinalTopPadding();
  float peeking_to_fullscreen_height =
      contents_view()->GetDisplayHeight() - kPeekingAppListHeight;
  float drag_amount = std::max(
      0, contents_view()->app_list_view()->GetWorkAreaBottom() -
             contents_view()->app_list_view()->app_list_y_position_in_screen());

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
