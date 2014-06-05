// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/app_list_folder_view.h"

#include <algorithm>

#include "grit/ui_strings.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_folder_item.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/views/app_list_item_view.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/apps_container_view.h"
#include "ui/app_list/views/apps_grid_view.h"
#include "ui/app_list/views/contents_view.h"
#include "ui/app_list/views/folder_background_view.h"
#include "ui/app_list/views/folder_header_view.h"
#include "ui/app_list/views/search_box_view.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/event.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/view_model.h"
#include "ui/views/view_model_utils.h"

namespace app_list {

namespace {

// Indexes of interesting views in ViewModel of AppListFolderView.
const int kIndexFolderHeader = 0;
const int kIndexChildItems = 1;

// Threshold for the distance from the center of the item to the circle of the
// folder container ink bubble, beyond which, the item is considered dragged
// out of the folder boundary.
const int kOutOfFolderContainerBubbleDelta = 30;

}  // namespace

AppListFolderView::AppListFolderView(AppsContainerView* container_view,
                                     AppListModel* model,
                                     AppListMainView* app_list_main_view)
    : container_view_(container_view),
      app_list_main_view_(app_list_main_view),
      folder_header_view_(new FolderHeaderView(this)),
      view_model_(new views::ViewModel),
      model_(model),
      folder_item_(NULL),
      hide_for_reparent_(false) {
  AddChildView(folder_header_view_);
  view_model_->Add(folder_header_view_, kIndexFolderHeader);

  items_grid_view_ = new AppsGridView(app_list_main_view_);
  items_grid_view_->set_folder_delegate(this);
  items_grid_view_->SetLayout(
      kPreferredIconDimension,
      container_view->apps_grid_view()->cols(),
      container_view->apps_grid_view()->rows_per_page());
  items_grid_view_->SetModel(model);
  AddChildView(items_grid_view_);
  view_model_->Add(items_grid_view_, kIndexChildItems);

  SetPaintToLayer(true);
  SetFillsBoundsOpaquely(false);

  model_->AddObserver(this);
}

AppListFolderView::~AppListFolderView() {
  model_->RemoveObserver(this);
}

void AppListFolderView::SetAppListFolderItem(AppListFolderItem* folder) {
  accessible_name_ = ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
      IDS_APP_LIST_FOLDER_OPEN_FOLDER_ACCESSIBILE_NAME);
  NotifyAccessibilityEvent(ui::AX_EVENT_ALERT, true);

  folder_item_ = folder;
  items_grid_view_->SetItemList(folder_item_->item_list());
  folder_header_view_->SetFolderItem(folder_item_);
}

void AppListFolderView::ScheduleShowHideAnimation(bool show,
                                                  bool hide_for_reparent) {
  hide_for_reparent_ = hide_for_reparent;

  // Stop any previous animation.
  layer()->GetAnimator()->StopAnimating();

  // Hide the top items temporarily if showing the view for opening the folder.
  if (show)
    items_grid_view_->SetTopItemViewsVisible(false);

  // Set initial state.
  layer()->SetOpacity(show ? 0.0f : 1.0f);
  SetVisible(true);
  UpdateFolderNameVisibility(true);

  ui::ScopedLayerAnimationSettings animation(layer()->GetAnimator());
  animation.SetTweenType(
      show ? kFolderFadeInTweenType : kFolderFadeOutTweenType);
  animation.AddObserver(this);
  animation.SetTransitionDuration(base::TimeDelta::FromMilliseconds(
      show ? kFolderTransitionInDurationMs : kFolderTransitionOutDurationMs));

  layer()->SetOpacity(show ? 1.0f : 0.0f);
}

gfx::Size AppListFolderView::GetPreferredSize() const {
  const gfx::Size header_size = folder_header_view_->GetPreferredSize();
  const gfx::Size grid_size = items_grid_view_->GetPreferredSize();
  int width = std::max(header_size.width(), grid_size.width());
  int height = header_size.height() + grid_size.height();
  return gfx::Size(width, height);
}

void AppListFolderView::Layout() {
  CalculateIdealBounds();
  views::ViewModelUtils::SetViewBoundsToIdealBounds(*view_model_);
}

bool AppListFolderView::OnKeyPressed(const ui::KeyEvent& event) {
  return items_grid_view_->OnKeyPressed(event);
}

void AppListFolderView::OnAppListItemWillBeDeleted(AppListItem* item) {
  if (item == folder_item_) {
    items_grid_view_->OnFolderItemRemoved();
    folder_header_view_->OnFolderItemRemoved();
    folder_item_ = NULL;

    // Do not change state if it is hidden.
    if (hide_for_reparent_ || layer()->opacity() == 0.0f)
      return;

    // If the folder item associated with this view is removed from the model,
    // (e.g. the last item in the folder was deleted), reset the view and signal
    // the container view to show the app list instead.
    // Pass NULL to ShowApps() to avoid triggering animation from the deleted
    // folder.
    container_view_->ShowApps(NULL);
  }
}

void AppListFolderView::OnImplicitAnimationsCompleted() {
  // Show the top items when the opening folder animation is done.
  if (layer()->opacity() == 1.0f)
    items_grid_view_->SetTopItemViewsVisible(true);

  // If the view is hidden for reparenting a folder item, it has to be visible,
  // so that drag_view_ can keep receiving mouse events.
  if (layer()->opacity() == 0.0f && !hide_for_reparent_)
    SetVisible(false);

  // Set the view bounds to a small rect, so that it won't overlap the root
  // level apps grid view during folder item reprenting transitional period.
  if (hide_for_reparent_)
    SetBoundsRect(gfx::Rect(bounds().x(), bounds().y(), 1, 1));
}

void AppListFolderView::CalculateIdealBounds() {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty())
    return;

  gfx::Rect header_frame(rect);
  gfx::Size size = folder_header_view_->GetPreferredSize();
  header_frame.set_height(size.height());
  view_model_->set_ideal_bounds(kIndexFolderHeader, header_frame);

  gfx::Rect grid_frame(rect);
  grid_frame.Subtract(header_frame);
  view_model_->set_ideal_bounds(kIndexChildItems, grid_frame);
}

void AppListFolderView::StartSetupDragInRootLevelAppsGridView(
    AppListItemView* original_drag_view,
    const gfx::Point& drag_point_in_root_grid) {
  // Converts the original_drag_view's bounds to the coordinate system of
  // root level grid view.
  gfx::RectF rect_f(original_drag_view->bounds());
  views::View::ConvertRectToTarget(items_grid_view_,
                                   container_view_->apps_grid_view(),
                                   &rect_f);
  gfx::Rect rect_in_root_grid_view = gfx::ToEnclosingRect(rect_f);

  container_view_->apps_grid_view()->
      InitiateDragFromReparentItemInRootLevelGridView(
          original_drag_view, rect_in_root_grid_view, drag_point_in_root_grid);
}

gfx::Rect AppListFolderView::GetItemIconBoundsAt(int index) {
  AppListItemView* item_view = items_grid_view_->GetItemViewAt(index);
  // Icon bounds relative to AppListItemView.
  const gfx::Rect icon_bounds = item_view->GetIconBounds();
  gfx::Rect to_apps_grid_view = item_view->ConvertRectToParent(icon_bounds);
  gfx::Rect to_folder =
      items_grid_view_->ConvertRectToParent(to_apps_grid_view);

  // Get the icon image's bound.
  to_folder.ClampToCenteredSize(
      gfx::Size(kPreferredIconDimension, kPreferredIconDimension));

  return to_folder;
}

void AppListFolderView::UpdateFolderViewBackground(bool show_bubble) {
  if (hide_for_reparent_)
    return;

  // Before showing the folder container inking bubble, hide the folder name.
  if (show_bubble)
    UpdateFolderNameVisibility(false);

  container_view_->folder_background_view()->UpdateFolderContainerBubble(
      show_bubble ? FolderBackgroundView::SHOW_BUBBLE :
                    FolderBackgroundView::HIDE_BUBBLE);
}

void AppListFolderView::UpdateFolderNameVisibility(bool visible) {
  folder_header_view_->UpdateFolderNameVisibility(visible);
}

bool AppListFolderView::IsPointOutsideOfFolderBoundary(
    const gfx::Point& point) {
  if (!GetLocalBounds().Contains(point))
    return true;

  gfx::Point center = GetLocalBounds().CenterPoint();
  float delta = (point - center).Length();
  return delta > container_view_->folder_background_view()->
      GetFolderContainerBubbleRadius() + kOutOfFolderContainerBubbleDelta;
}

// When user drags a folder item out of the folder boundary ink bubble, the
// folder view UI will be hidden, and switch back to top level AppsGridView.
// The dragged item will seamlessly move on the top level AppsGridView.
// In order to achieve the above, we keep the folder view and its child grid
// view visible with opacity 0, so that the drag_view_ on the hidden grid view
// will keep receiving mouse event. At the same time, we initiated a new
// drag_view_ in the top level grid view, and keep it moving with the hidden
// grid view's drag_view_, so that the dragged item can be engaged in drag and
// drop flow in the top level grid view. During the reparenting process, the
// drag_view_ in hidden grid view will dispatch the drag and drop event to
// the top level grid view, until the drag ends.
void AppListFolderView::ReparentItem(
    AppListItemView* original_drag_view,
    const gfx::Point& drag_point_in_folder_grid) {
  // Convert the drag point relative to the root level AppsGridView.
  gfx::Point to_root_level_grid = drag_point_in_folder_grid;
  ConvertPointToTarget(items_grid_view_,
                       container_view_->apps_grid_view(),
                       &to_root_level_grid);
  StartSetupDragInRootLevelAppsGridView(original_drag_view, to_root_level_grid);
  container_view_->ReparentFolderItemTransit(folder_item_);
}

void AppListFolderView::DispatchDragEventForReparent(
    AppsGridView::Pointer pointer,
    const gfx::Point& drag_point_in_folder_grid) {
  AppsGridView* root_grid = container_view_->apps_grid_view();
  gfx::Point drag_point_in_root_grid = drag_point_in_folder_grid;
  ConvertPointToTarget(items_grid_view_, root_grid, &drag_point_in_root_grid);
  root_grid->UpdateDragFromReparentItem(pointer, drag_point_in_folder_grid);
}

void AppListFolderView::DispatchEndDragEventForReparent(
    bool events_forwarded_to_drag_drop_host,
    bool cancel_drag) {
  container_view_->apps_grid_view()->EndDragFromReparentItemInRootLevel(
      events_forwarded_to_drag_drop_host, cancel_drag);
}

void AppListFolderView::HideViewImmediately() {
  SetVisible(false);
  hide_for_reparent_ = false;
}

void AppListFolderView::CloseFolderPage() {
  accessible_name_ = ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
      IDS_APP_LIST_FOLDER_CLOSE_FOLDER_ACCESSIBILE_NAME);
  NotifyAccessibilityEvent(ui::AX_EVENT_ALERT, true);

  GiveBackFocusToSearchBox();
  if (items_grid_view()->dragging())
    items_grid_view()->EndDrag(true);
  items_grid_view()->ClearAnySelectedView();
  container_view_->ShowApps(folder_item_);
}

bool AppListFolderView::IsOEMFolder() const {
  return folder_item_->folder_type() == AppListFolderItem::FOLDER_TYPE_OEM;
}

void AppListFolderView::SetRootLevelDragViewVisible(bool visible) {
  container_view_->apps_grid_view()->SetDragViewVisible(visible);
}

void AppListFolderView::GetAccessibleState(ui::AXViewState* state) {
  state->role = ui::AX_ROLE_BUTTON;
  state->name = accessible_name_;
}

void AppListFolderView::NavigateBack(AppListFolderItem* item,
                                     const ui::Event& event_flags) {
  CloseFolderPage();
}

void AppListFolderView::GiveBackFocusToSearchBox() {
  app_list_main_view_->search_box_view()->search_box()->RequestFocus();
}

void AppListFolderView::SetItemName(AppListFolderItem* item,
                                    const std::string& name) {
  model_->SetItemName(item, name);
}

}  // namespace app_list
