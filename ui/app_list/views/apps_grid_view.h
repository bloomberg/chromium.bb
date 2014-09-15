// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_APPS_GRID_VIEW_H_
#define UI_APP_LIST_VIEWS_APPS_GRID_VIEW_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/timer/timer.h"
#include "ui/app_list/app_list_export.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/app_list_model_observer.h"
#include "ui/app_list/pagination_model.h"
#include "ui/app_list/pagination_model_observer.h"
#include "ui/base/models/list_model_observer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"
#include "ui/views/view_model.h"

#if defined(OS_WIN)
#include "ui/base/dragdrop/drag_source_win.h"
#endif

namespace views {
class ButtonListener;
class DragImageView;
}

namespace app_list {

#if defined(OS_WIN)
class SynchronousDrag;
#endif

namespace test {
class AppsGridViewTestApi;
}

class ApplicationDragAndDropHost;
class AppListItemView;
class AppsGridViewDelegate;
class AppsGridViewFolderDelegate;
class PageSwitcher;
class PaginationController;

// AppsGridView displays a grid for AppListItemList sub model.
class APP_LIST_EXPORT AppsGridView : public views::View,
                                     public views::ButtonListener,
                                     public AppListItemListObserver,
                                     public PaginationModelObserver,
                                     public AppListModelObserver,
                                     public ui::ImplicitAnimationObserver {
 public:
  enum Pointer {
    NONE,
    MOUSE,
    TOUCH,
  };

  // Constructs the app icon grid view. |delegate| is the delegate of this
  // view, which usually is the hosting AppListView.
  explicit AppsGridView(AppsGridViewDelegate* delegate);
  virtual ~AppsGridView();

  // Sets fixed layout parameters. After setting this, CalculateLayout below
  // is no longer called to dynamically choosing those layout params.
  void SetLayout(int cols, int rows_per_page);

  int cols() const { return cols_; }
  int rows_per_page() const { return rows_per_page_; }

  // This resets the grid view to a fresh state for showing the app list.
  void ResetForShowApps();

  // Sets |model| to use. Note this does not take ownership of |model|.
  void SetModel(AppListModel* model);

  // Sets the |item_list| to render. Note this does not take ownership of
  // |item_list|.
  void SetItemList(AppListItemList* item_list);

  void SetSelectedView(views::View* view);
  void ClearSelectedView(views::View* view);
  void ClearAnySelectedView();
  bool IsSelectedView(const views::View* view) const;

  // Ensures the view is visible. Note that if there is a running page
  // transition, this does nothing.
  void EnsureViewVisible(const views::View* view);

  void InitiateDrag(AppListItemView* view,
                    Pointer pointer,
                    const ui::LocatedEvent& event);

  // Called from AppListItemView when it receives a drag event. Returns true
  // if the drag is still happening.
  bool UpdateDragFromItem(Pointer pointer, const ui::LocatedEvent& event);

  // Called when the user is dragging an app. |point| is in grid view
  // coordinates.
  void UpdateDrag(Pointer pointer, const gfx::Point& point);
  void EndDrag(bool cancel);
  bool IsDraggedView(const views::View* view) const;
  void ClearDragState();
  void SetDragViewVisible(bool visible);

  // Set the drag and drop host for application links.
  void SetDragAndDropHostOfCurrentAppList(
      ApplicationDragAndDropHost* drag_and_drop_host);

  // Prerenders the icons on and around the currently selected page.
  void Prerender();

  // Return true if the |bounds_animator_| is animating |view|.
  bool IsAnimatingView(views::View* view);

  bool has_dragged_view() const { return drag_view_ != NULL; }
  bool dragging() const { return drag_pointer_ != NONE; }

  // Gets the PaginationModel used for the grid view.
  PaginationModel* pagination_model() { return &pagination_model_; }

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual bool OnKeyPressed(const ui::KeyEvent& event) OVERRIDE;
  virtual bool OnKeyReleased(const ui::KeyEvent& event) OVERRIDE;
  virtual bool OnMouseWheel(const ui::MouseWheelEvent& event) OVERRIDE;
  virtual void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) OVERRIDE;
  virtual bool GetDropFormats(
      int* formats,
      std::set<OSExchangeData::CustomFormat>* custom_formats) OVERRIDE;
  virtual bool CanDrop(const OSExchangeData& data) OVERRIDE;
  virtual int OnDragUpdated(const ui::DropTargetEvent& event) OVERRIDE;

  // Overridden from ui::EventHandler:
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;
  virtual void OnScrollEvent(ui::ScrollEvent* event) OVERRIDE;

  // Stops the timer that triggers a page flip during a drag.
  void StopPageFlipTimer();

  // Returns the item view of the item at |index|.
  AppListItemView* GetItemViewAt(int index) const;

  // Show or hide the top item views.
  void SetTopItemViewsVisible(bool visible);

  // Schedules an animation to show or hide the view.
  void ScheduleShowHideAnimation(bool show);

  // Called to initiate drag for reparenting a folder item in root level grid
  // view.
  // Both |drag_view_rect| and |drag_pint| is in the coordinates of root level
  // grid view.
  void InitiateDragFromReparentItemInRootLevelGridView(
      AppListItemView* original_drag_view,
      const gfx::Rect& drag_view_rect,
      const gfx::Point& drag_point);

  // Updates drag in the root level grid view when receiving the drag event
  // dispatched from the hidden grid view for reparenting a folder item.
  void UpdateDragFromReparentItem(Pointer pointer,
                                  const gfx::Point& drag_point);

  // Dispatches the drag event from hidden grid view to the top level grid view.
  void DispatchDragEventForReparent(Pointer pointer,
                                    const gfx::Point& drag_point);

  // Handles EndDrag event dispatched from the hidden folder grid view in the
  // root level grid view to end reparenting a folder item.
  // |events_forwarded_to_drag_drop_host|: True if the dragged item is dropped
  // to the drag_drop_host, eg. dropped on shelf.
  // |cancel_drag|: True if the drag is ending because it has been canceled.
  void EndDragFromReparentItemInRootLevel(
      bool events_forwarded_to_drag_drop_host,
      bool cancel_drag);

  // Handles EndDrag event in the hidden folder grid view to end reparenting
  // a folder item.
  void EndDragForReparentInHiddenFolderGridView();

  // Called when the folder item associated with the grid view is removed.
  // The grid view must be inside a folder view.
  void OnFolderItemRemoved();

  // Return the view model for test purposes.
  const views::ViewModel* view_model_for_test() const { return &view_model_; }

  // For test: Return if the drag and drop handler was set.
  bool has_drag_and_drop_host_for_test() { return NULL != drag_and_drop_host_; }

  // For test: Return if the drag and drop operation gets dispatched.
  bool forward_events_to_drag_and_drop_host_for_test() {
    return forward_events_to_drag_and_drop_host_;
  }

  void set_folder_delegate(AppsGridViewFolderDelegate* folder_delegate) {
    folder_delegate_ = folder_delegate;
  }

  AppListItemView* activated_folder_item_view() const {
    return activated_folder_item_view_;
  }

  const AppListModel* model() const { return model_; }

 private:
  friend class test::AppsGridViewTestApi;

  enum DropAttempt {
    DROP_FOR_NONE,
    DROP_FOR_REORDER,
    DROP_FOR_FOLDER,
  };

  // Represents the index to an item view in the grid.
  struct Index {
    Index() : page(-1), slot(-1) {}
    Index(int page, int slot) : page(page), slot(slot) {}

    bool operator==(const Index& other) const {
      return page == other.page && slot == other.slot;
    }
    bool operator!=(const Index& other) const {
      return page != other.page || slot != other.slot;
    }
    bool operator<(const Index& other) const {
      if (page != other.page)
        return page < other.page;

      return slot < other.slot;
    }

    int page;  // Which page an item view is on.
    int slot;  // Which slot in the page an item view is in.
  };

  int tiles_per_page() const { return cols_ * rows_per_page_; }

  // Updates from model.
  void Update();

  // Updates page splits for item views.
  void UpdatePaging();

  // Updates the number of pulsing block views based on AppListModel status and
  // number of apps.
  void UpdatePulsingBlockViews();

  views::View* CreateViewForItemAtIndex(size_t index);

  // Convert between the model index and the visual index. The model index
  // is the index of the item in AppListModel. The visual index is the Index
  // struct above with page/slot info of where to display the item.
  Index GetIndexFromModelIndex(int model_index) const;
  int GetModelIndexFromIndex(const Index& index) const;

  void SetSelectedItemByIndex(const Index& index);
  bool IsValidIndex(const Index& index) const;

  Index GetIndexOfView(const views::View* view) const;
  views::View* GetViewAtIndex(const Index& index) const;

  // Gets the index of the AppListItemView at the end of the view model.
  Index GetLastViewIndex() const;

  void MoveSelected(int page_delta, int slot_x_delta, int slot_y_delta);

  void CalculateIdealBounds();
  void AnimateToIdealBounds();

  // Invoked when the given |view|'s current bounds and target bounds are on
  // different rows. To avoid moving diagonally, |view| would be put into a
  // slot prior |target| and fade in while moving to |target|. In the meanwhile,
  // a layer copy of |view| would start at |current| and fade out while moving
  // to succeeding slot of |current|. |animate_current| controls whether to run
  // fading out animation from |current|. |animate_target| controls whether to
  // run fading in animation to |target|.
  void AnimationBetweenRows(views::View* view,
                            bool animate_current,
                            const gfx::Rect& current,
                            bool animate_target,
                            const gfx::Rect& target);

  // Extracts drag location info from |event| into |drag_point|.
  void ExtractDragLocation(const ui::LocatedEvent& event,
                           gfx::Point* drag_point);

  // Updates |reorder_drop_target_|, |folder_drop_target_| and |drop_attempt_|
  // based on |drag_view_|'s position.
  void CalculateDropTarget();

  // If |point| is a valid folder drop target, returns true and sets
  // |drop_target| to the index of the view to do a folder drop for.
  bool CalculateFolderDropTarget(const gfx::Point& point,
                                 Index* drop_target) const;

  // Calculates the reorder target |point| and sets |drop_target| to the index
  // of the view to reorder.
  void CalculateReorderDropTarget(const gfx::Point& point,
                                  Index* drop_target) const;

  // Prepares |drag_and_drop_host_| for dragging. |grid_location| contains
  // the drag point in this grid view's coordinates.
  void StartDragAndDropHostDrag(const gfx::Point& grid_location);

  // Dispatch the drag and drop update event to the dnd host (if needed).
  void DispatchDragEventToDragAndDropHost(
      const gfx::Point& location_in_screen_coordinates);

  // Starts the page flip timer if |drag_point| is in left/right side page flip
  // zone or is over page switcher.
  void MaybeStartPageFlipTimer(const gfx::Point& drag_point);

  // Invoked when |page_flip_timer_| fires.
  void OnPageFlipTimer();

  // Updates |model_| to move item represented by |item_view| to |target| slot.
  void MoveItemInModel(views::View* item_view, const Index& target);

  // Updates |model_| to move item represented by |item_view| into a folder
  // containing item located at |target| slot, also update |view_model_| for
  // the related view changes.
  void MoveItemToFolder(views::View* item_view, const Index& target);

  // Updates both data model and view_model_ for re-parenting a folder item to a
  // new position in top level item list.
  void ReparentItemForReorder(views::View* item_view, const Index& target);

  // Updates both data model and view_model_ for re-parenting a folder item
  // to anther folder target.
  void ReparentItemToAnotherFolder(views::View* item_view, const Index& target);

  // If there is only 1 item left in the source folder after reparenting an item
  // from it, updates both data model and view_model_ for removing last item
  // from the source folder and removes the source folder.
  void RemoveLastItemFromReparentItemFolderIfNecessary(
      const std::string& source_folder_id);

  // If user does not drop the re-parenting folder item to any valid target,
  // cancel the re-parenting action, let the item go back to its original
  // parent folder with UI animation.
  void CancelFolderItemReparent(AppListItemView* drag_item_view);

  // Cancels any context menus showing for app items on the current page.
  void CancelContextMenusOnCurrentPage();

  // Removes the AppListItemView at |index| in |view_model_| and deletes it.
  void DeleteItemViewAtIndex(int index);

  // Returns true if |point| lies within the bounds of this grid view plus a
  // buffer area surrounding it.
  bool IsPointWithinDragBuffer(const gfx::Point& point) const;

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // Overridden from AppListItemListObserver:
  virtual void OnListItemAdded(size_t index, AppListItem* item) OVERRIDE;
  virtual void OnListItemRemoved(size_t index, AppListItem* item) OVERRIDE;
  virtual void OnListItemMoved(size_t from_index,
                               size_t to_index,
                               AppListItem* item) OVERRIDE;

  // Overridden from PaginationModelObserver:
  virtual void TotalPagesChanged() OVERRIDE;
  virtual void SelectedPageChanged(int old_selected, int new_selected) OVERRIDE;
  virtual void TransitionStarted() OVERRIDE;
  virtual void TransitionChanged() OVERRIDE;

  // Overridden from AppListModelObserver:
  virtual void OnAppListModelStatusChanged() OVERRIDE;

  // ui::ImplicitAnimationObserver overrides:
  virtual void OnImplicitAnimationsCompleted() OVERRIDE;

  // Hide a given view temporarily without losing (mouse) events and / or
  // changing the size of it. If |immediate| is set the change will be
  // immediately applied - otherwise it will change gradually.
  // If |hide| is set the view will get hidden, otherwise it gets shown.
  void SetViewHidden(views::View* view, bool hide, bool immediate);

  // Whether the folder drag-and-drop UI should be enabled.
  bool EnableFolderDragDropUI();

  // Whether target specified by |drap_target| can accept more items to be
  // dropped into it.
  bool CanDropIntoTarget(const Index& drop_target) const;

  // Returns the size of the entire tile grid.
  gfx::Size GetTileGridSize() const;

  // Returns the slot number which the given |point| falls into or the closest
  // slot if |point| is outside the page's bounds.
  Index GetNearestTileIndexForPoint(const gfx::Point& point) const;

  // Gets the bounds of the tile located at |slot| on the current page.
  gfx::Rect GetExpectedTileBounds(int slot) const;

  // Gets the bounds of the tile located at |row| and |col| on the current page.
  gfx::Rect GetExpectedTileBounds(int row, int col) const;

  // Gets the item view located at |slot| on the current page. If there is
  // no item located at |slot|, returns NULL.
  views::View* GetViewAtSlotOnCurrentPage(int slot);

  // Sets state of the view with |target_index| to |is_target_folder| for
  // dropping |drag_view_|.
  void SetAsFolderDroppingTarget(const Index& target_index,
                                 bool is_target_folder);

  // Invoked when |reorder_timer_| fires to show re-order preview UI.
  void OnReorderTimer();

  // Invoked when |folder_item_reparent_timer_| fires.
  void OnFolderItemReparentTimer();

  // Invoked when |folder_dropping_timer_| fires to show folder dropping
  // preview UI.
  void OnFolderDroppingTimer();

  // Updates drag state for dragging inside a folder's grid view.
  void UpdateDragStateInsideFolder(Pointer pointer,
                                   const gfx::Point& drag_point);

  // Returns true if drag event is happening in the root level AppsGridView
  // for reparenting a folder item.
  bool IsDraggingForReparentInRootLevelGridView() const;

  // Returns true if drag event is happening in the hidden AppsGridView of the
  // folder during reparenting a folder item.
  bool IsDraggingForReparentInHiddenGridView() const;

  // Returns the target icon bounds for |drag_item_view| to fly back
  // to its parent |folder_item_view| in animation.
  gfx::Rect GetTargetIconRectInFolder(AppListItemView* drag_item_view,
      AppListItemView* folder_item_view);

  // Returns true if the grid view is under an OEM folder.
  bool IsUnderOEMFolder();

  void StartSettingUpSynchronousDrag();
  bool RunSynchronousDrag();
  void CleanUpSynchronousDrag();
#if defined(OS_WIN)
  void OnGotShortcutPath(scoped_refptr<SynchronousDrag> drag,
                         const base::FilePath& path);
#endif

  AppListModel* model_;  // Owned by AppListView.
  AppListItemList* item_list_;  // Not owned.
  AppsGridViewDelegate* delegate_;

  // This can be NULL. Only grid views inside folders have a folder delegate.
  AppsGridViewFolderDelegate* folder_delegate_;

  PaginationModel pagination_model_;
  // Must appear after |pagination_model_|.
  scoped_ptr<PaginationController> pagination_controller_;
  PageSwitcher* page_switcher_view_;  // Owned by views hierarchy.

  int cols_;
  int rows_per_page_;

  // Tracks app item views. There is a view per item in |model_|.
  views::ViewModel view_model_;

  // Tracks pulsing block views.
  views::ViewModel pulsing_blocks_model_;

  views::View* selected_view_;

  AppListItemView* drag_view_;

  // The index of the drag_view_ when the drag starts.
  Index drag_view_init_index_;

  // The point where the drag started in AppListItemView coordinates.
  gfx::Point drag_view_offset_;

  // The point where the drag started in GridView coordinates.
  gfx::Point drag_start_grid_view_;

  // The location of |drag_view_| when the drag started.
  gfx::Point drag_view_start_;

  // Page the drag started on.
  int drag_start_page_;

#if defined(OS_WIN)
  // Created when a drag is started (ie: drag exceeds the drag threshold), but
  // not Run() until supplied with a shortcut path.
  scoped_refptr<SynchronousDrag> synchronous_drag_;

  // Whether to use SynchronousDrag to support dropping to task bar etc.
  bool use_synchronous_drag_;
#endif

  Pointer drag_pointer_;

  // The most recent reorder drop target.
  Index reorder_drop_target_;

  // The most recent folder drop target.
  Index folder_drop_target_;

  // The index where an empty slot has been left as a placeholder for the
  // reorder drop target. This updates when the reorder animation triggers.
  Index reorder_placeholder_;

  // The current action that ending a drag will perform.
  DropAttempt drop_attempt_;

  // Timer for re-ordering the |drop_target_| and |drag_view_|.
  base::OneShotTimer<AppsGridView> reorder_timer_;

  // Timer for dropping |drag_view_| into the folder containing
  // the |drop_target_|.
  base::OneShotTimer<AppsGridView> folder_dropping_timer_;

  // Timer for dragging a folder item out of folder container ink bubble.
  base::OneShotTimer<AppsGridView> folder_item_reparent_timer_;

  // An application target drag and drop host which accepts dnd operations.
  ApplicationDragAndDropHost* drag_and_drop_host_;

  // The drag operation is currently inside the dnd host and events get
  // forwarded.
  bool forward_events_to_drag_and_drop_host_;

  // Last mouse drag location in this view's coordinates.
  gfx::Point last_drag_point_;

  // Timer to auto flip page when dragging an item near the left/right edges.
  base::OneShotTimer<AppsGridView> page_flip_timer_;

  // Target page to switch to when |page_flip_timer_| fires.
  int page_flip_target_;

  // Delay in milliseconds of when |page_flip_timer_| should fire after user
  // drags an item near the edges.
  int page_flip_delay_in_ms_;

  views::BoundsAnimator bounds_animator_;

  // The most recent activated folder item view.
  AppListItemView* activated_folder_item_view_;

  // Tracks if drag_view_ is dragged out of the folder container bubble
  // when dragging a item inside a folder.
  bool drag_out_of_folder_container_;

  // True if the drag_view_ item is a folder item being dragged for reparenting.
  bool dragging_for_reparent_item_;

  DISALLOW_COPY_AND_ASSIGN(AppsGridView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_APPS_GRID_VIEW_H_
