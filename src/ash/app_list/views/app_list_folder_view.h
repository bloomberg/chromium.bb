// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APP_LIST_FOLDER_VIEW_H_
#define ASH_APP_LIST_VIEWS_APP_LIST_FOLDER_VIEW_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/model/app_list_item_list_observer.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/model/app_list_model_observer.h"
#include "ash/app_list/views/apps_grid_view.h"
#include "ash/app_list/views/apps_grid_view_folder_delegate.h"
#include "ash/app_list/views/folder_header_view.h"
#include "ash/app_list/views/folder_header_view_delegate.h"
#include "ash/app_list/views/paged_apps_grid_view.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/compositor/throughput_tracker.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace ash {

class AppListA11yAnnouncer;
class AppListFolderController;
class AppListFolderItem;
class AppListItemView;
class AppListModel;
class AppListViewDelegate;
class AppsContainerView;
class AppsGridView;
class FolderHeaderView;
class PageSwitcher;
class ScrollViewGradientHelper;

// Displays folder contents via an AppsGridView. App items can be dragged out
// of the folder to the main apps grid.
class ASH_EXPORT AppListFolderView
    : public views::View,
      public FolderHeaderViewDelegate,
      public AppListModelProvider::Observer,
      public AppListModelObserver,
      public views::ViewObserver,
      public AppsGridViewFolderDelegate,
      public PagedAppsGridView::ContainerDelegate {
 public:
  METADATA_HEADER(AppListFolderView);

  // The maximum number of columns a folder can have.
  static constexpr int kMaxFolderColumns = 4;

  // When using paged folder item grid, the maximum number of rows a folder
  // items grid can have.
  static constexpr int kMaxPagedFolderRows = 4;

  AppListFolderView(AppListFolderController* folder_controller,
                    AppsGridView* root_apps_grid_view,
                    ContentsView* contents_view,
                    AppListA11yAnnouncer* a11y_announcer,
                    AppListViewDelegate* view_delegate);
  AppListFolderView(const AppListFolderView&) = delete;
  AppListFolderView& operator=(const AppListFolderView&) = delete;
  ~AppListFolderView() override;

  // An interface for the folder opening and closing animations.
  class Animation {
   public:
    virtual ~Animation() = default;
    // `completion_callback` is an optional callback to be run when the
    // animation completes. Not run if the animation gets reset before
    // completion.
    virtual void ScheduleAnimation(base::OnceClosure completion_callback) = 0;
    virtual bool IsAnimationRunning() = 0;
  };

  // Sets the `AppListConfig` that should be used to configure app list item
  // size within the folder items grid.
  void UpdateAppListConfig(const AppListConfig* config);

  // Configures AppListFolderView to show the contents for the folder item
  // associated with `folder_item_view`. The folder view will be anchored at
  // `folder_item_view`.
  void ConfigureForFolderItemView(AppListItemView* folder_item_view);

  // Schedules an animation to show or hide the view.
  // If |show| is false, the view should be set to invisible after the
  // animation is done unless |hide_for_reparent| is true.
  void ScheduleShowHideAnimation(bool show, bool hide_for_reparent);

  // Hides the view immediately without animation.
  void HideViewImmediately();

  // Prepares folder item grid for closing the folder - it ends any in-progress
  // drag, and clears any selected view.
  void ResetItemsGridForClose();

  // Closes the folder page and goes back the top level page.
  void CloseFolderPage();

  // Focuses the first app item. Does not set the selection or perform a11y
  // announce if `silently` is true.
  void FocusFirstItem(bool silently);

  // views::View
  void Layout() override;
  void ChildPreferredSizeChanged(View* child) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // AppListModelProvider::Observer:
  void OnActiveAppListModelsChanged(AppListModel* model,
                                    SearchModel* search_model) override;

  // views::ViewObserver:
  void OnViewIsDeleting(views::View* view) override;

  // AppListModelObserver
  void OnAppListItemWillBeDeleted(AppListItem* item) override;

  // Updates preferred bounds of this view based on the activated folder item
  // icon's bounds.
  void UpdatePreferredBounds();

  // Returns the Y-offset that would move a folder out from under a visible
  // Virtual keyboard
  int GetYOffsetForFolder();

  // Returns true if this view's child views are in animation for opening or
  // closing the folder.
  bool IsAnimationRunning() const;

  // Sets the bounding box for the folder view bounds. The bounds are expected
  // to be in the parent view's coordinate system.
  void SetBoundingBox(const gfx::Rect& bounding_box);

  AppsGridView* items_grid_view() { return items_grid_view_; }

  FolderHeaderView* folder_header_view() { return folder_header_view_; }

  views::View* background_view() { return background_view_; }

  views::View* contents_container() { return contents_container_; }

  const AppListFolderItem* folder_item() const { return folder_item_; }

  const gfx::Rect& folder_item_icon_bounds() const {
    return folder_item_icon_bounds_;
  }

  const gfx::Rect& preferred_bounds() const { return preferred_bounds_; }

  // Records the smoothness of folder show/hide animations mixed with the
  // BackgroundAnimation, FolderItemTitleAnimation, TopIconAnimation, and
  // ContentsContainerAnimation.
  void RecordAnimationSmoothness();

  // Called when tablet mode starts and ends.
  void OnTabletModeChanged(bool started);

  // views::View:
  void OnScrollEvent(ui::ScrollEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;

  // Overridden from FolderHeaderViewDelegate:
  void SetItemName(AppListFolderItem* item, const std::string& name) override;

  // Overridden from AppsGridViewFolderDelegate:
  void ReparentItem(AppListItemView* original_drag_view,
                    const gfx::Point& drag_point_in_folder_grid) override;
  void DispatchDragEventForReparent(
      AppsGridView::Pointer pointer,
      const gfx::Point& drag_point_in_folder_grid) override;
  void DispatchEndDragEventForReparent(
      bool events_forwarded_to_drag_drop_host,
      bool cancel_drag,
      std::unique_ptr<AppDragIconProxy> drag_icon_proxy) override;
  bool IsDragPointOutsideOfFolder(const gfx::Point& drag_point) override;
  bool IsOEMFolder() const override;
  void HandleKeyboardReparent(AppListItemView* reparented_view,
                              ui::KeyboardCode key_code) override;

  // PagedAppsGridView::ContainerDelegate:
  bool IsPointWithinPageFlipBuffer(const gfx::Point& point) const override;
  bool IsPointWithinBottomDragBuffer(const gfx::Point& point,
                                     int page_flip_zone_size) const override;

  const AppListConfig* GetAppListConfig() const;

  views::ScrollView* scroll_view_for_test() { return scroll_view_; }

 private:
  // Creates an apps grid view with fixed-size pages.
  void CreatePagedAppsGrid(ContentsView* contents_view);

  // Creates a vertically scrollable apps grid view.
  void CreateScrollableAppsGrid();

  // Returns the compositor associated to the widget containing this view.
  // Returns nullptr if there isn't one associated with this widget.
  ui::Compositor* GetCompositor();

  // Called from the root apps grid view to cancel reparent drag from the root
  // apps grid.
  void CancelReparentDragFromRootGrid();

  // Calculates whether the folder would fit in the bounding box if it had the
  // max allowed number of rows, and condenses the margins between grid items if
  // this is not the case. The goal is to prevent a portion of folder UI from
  // getting laid out outside the bounding box. Tile size scaling done for the
  // top level apps grid should ensure the folder UI reasonably fits within the
  // bounding box with no item margins. At certain screen sizes, this approach
  // also fails, but at that point the top level apps grid doesn't work too well
  // either.
  // No-op if the productivity launcher is enabled, in which case folder grid is
  // scrollable, and should handle the case where grid bounds overflow bounding
  // box size gracefully.
  void ShrinkGridTileMarginsWhenNeeded();

  // Resets the folder view state. Called when the folder view gets hidden (and
  // hide animations finish) to disassociate the folder view with the current
  // folder item (if any).
  // `restore_folder_item_view_state` - whether the folder item view state
  // should be restored to the default state (icon and title shown). Set to
  // false when resetting the folder state due to folder item view deletion.
  void ResetState(bool restore_folder_item_view_state);

  // Controller interface implemented by the container for this view.
  AppListFolderController* const folder_controller_;

  // The root (non-folder) apps grid view.
  AppsGridView* const root_apps_grid_view_;

  // Used to send accessibility alerts. Owned by the parent apps container.
  AppListA11yAnnouncer* const a11y_announcer_;

  // The view is used to draw a background with corner radius.
  views::View* background_view_;  // Owned by views hierarchy.

  // The view is used as a container for all following views.
  views::View* contents_container_;  // Owned by views hierarchy.

  FolderHeaderView* folder_header_view_;  // Owned by views hierarchy.
  AppsGridView* items_grid_view_;         // Owned by views hierarchy.

  // Only used for non-ProductivityLauncher. Owned by views hierarchy.
  PageSwitcher* page_switcher_ = nullptr;

  // Only used for ProductivityLauncher. Owned by views hierarchy.
  views::ScrollView* scroll_view_ = nullptr;

  // Adds fade in/out gradients to `scroll_view_`.
  // Only used for ProductivityLauncher.
  std::unique_ptr<ScrollViewGradientHelper> gradient_helper_;

  AppListViewDelegate* const view_delegate_;
  AppListFolderItem* folder_item_ = nullptr;  // Not owned.

  // The folder item in the root apps grid associated with this folder.
  AppListItemView* folder_item_view_ = nullptr;

  // The bounds of the activated folder item icon relative to this view.
  gfx::Rect folder_item_icon_bounds_;

  // The preferred bounds of this view relative to AppsContainerView.
  gfx::Rect preferred_bounds_;

  // The bounds of the box within which the folder view can be shown. The bounds
  // are relative the the parent view's coordinate system.
  gfx::Rect bounding_box_;

  bool hide_for_reparent_ = false;

  std::vector<std::unique_ptr<Animation>> folder_visibility_animations_;

  // Records smoothness of the folder show/hide animation.
  absl::optional<ui::ThroughputTracker> show_hide_metrics_tracker_;

  base::ScopedObservation<AppListModel, AppListModelObserver>
      model_observation_{this};

  // Observes `folder_item_view_` deletion, so the folder state can be cleared
  // if the folder item view is destroyed (for example, the view may get deleted
  // during folder hide animation if the backing item gets deleted from the
  // model, and animations depend on the folder item view).
  base::ScopedObservation<views::View, views::ViewObserver>
      folder_item_view_observer_{this};

  base::WeakPtrFactory<AppListFolderView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APP_LIST_FOLDER_VIEW_H_
