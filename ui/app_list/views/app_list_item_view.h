// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_APP_LIST_ITEM_VIEW_H_
#define UI_APP_LIST_VIEWS_APP_LIST_ITEM_VIEW_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer/timer.h"
#include "ui/app_list/app_list_export.h"
#include "ui/app_list/app_list_item_observer.h"
#include "ui/app_list/views/cached_label.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/custom_button.h"

class SkBitmap;

namespace views {
class ImageView;
class Label;
class MenuRunner;
}

namespace app_list {

class AppListItem;
class AppsGridView;
class ProgressBarView;

class APP_LIST_EXPORT AppListItemView : public views::CustomButton,
                                        public views::ContextMenuController,
                                        public AppListItemObserver {
 public:
  // Internal class name.
  static const char kViewClassName[];

  AppListItemView(AppsGridView* apps_grid_view, AppListItem* item);
  virtual ~AppListItemView();

  void Prerender();

  void CancelContextMenu();

  gfx::ImageSkia GetDragImage();
  void OnDragEnded();
  gfx::Point GetDragImageOffset();

  void SetAsAttemptedFolderTarget(bool is_target_folder);

  AppListItem* item() const { return item_; }

  views::ImageView* icon() const { return icon_; }

  const views::Label* title() const { return title_; }

  // In a synchronous drag the item view isn't informed directly of the drag
  // ending, so the runner of the drag should call this.
  void OnSyncDragEnd();

  // Returns the icon bounds relative to AppListItemView.
  const gfx::Rect& GetIconBounds() const;

  // Sets UI state to dragging state.
  void SetDragUIState();

  // Returns the icon bounds for the given |target_bounds| as
  // the assuming bounds of this view.
  gfx::Rect GetIconBoundsForTargetViewBounds(const gfx::Rect& target_bounds);

 private:
  enum UIState {
    UI_STATE_NORMAL,    // Normal UI (icon + label)
    UI_STATE_DRAGGING,  // Dragging UI (scaled icon only)
    UI_STATE_DROPPING_IN_FOLDER,  // Folder dropping preview UI
  };

  // Get icon from |item_| and schedule background processing.
  void UpdateIcon();

  // Update the tooltip text from |item_|.
  void UpdateTooltip();

  void SetUIState(UIState state);

  // Sets |touch_dragging_| flag and updates UI.
  void SetTouchDragging(bool touch_dragging);

  // Invoked when |mouse_drag_timer_| fires to show dragging UI.
  void OnMouseDragTimer();

  // If the item is not in a folder, not highlighted, not being dragged, and not
  // having something dropped onto it, enables subpixel AA for the title.
  void SetTitleSubpixelAA();

  // AppListItemObserver overrides:
  virtual void ItemIconChanged() OVERRIDE;
  virtual void ItemNameChanged() OVERRIDE;
  virtual void ItemHighlightedChanged() OVERRIDE;
  virtual void ItemIsInstallingChanged() OVERRIDE;
  virtual void ItemPercentDownloadedChanged() OVERRIDE;

  // views::View overrides:
  virtual const char* GetClassName() const OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void SchedulePaintInRect(const gfx::Rect& r) OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

  // views::ContextMenuController overrides:
  virtual void ShowContextMenuForView(views::View* source,
                                      const gfx::Point& point,
                                      ui::MenuSourceType source_type) OVERRIDE;

  // views::CustomButton overrides:
  virtual void StateChanged() OVERRIDE;
  virtual bool ShouldEnterPushedState(const ui::Event& event) OVERRIDE;

  // views::View overrides:
  virtual bool OnKeyPressed(const ui::KeyEvent& event) OVERRIDE;
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseCaptureLost() OVERRIDE;
  virtual bool OnMouseDragged(const ui::MouseEvent& event) OVERRIDE;

  // ui::EventHandler overrides:
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

  AppListItem* item_;  // Owned by AppListModel.

  AppsGridView* apps_grid_view_;  // Owned by views hierarchy.
  views::ImageView* icon_;  // Owned by views hierarchy.
  CachedLabel* title_;  // Owned by views hierarchy.
  ProgressBarView* progress_bar_;  // Owned by views hierarchy.

  scoped_ptr<views::MenuRunner> context_menu_runner_;

  UIState ui_state_;

  // True if scroll gestures should contribute to dragging.
  bool touch_dragging_;

  // A timer to defer showing drag UI when mouse is pressed.
  base::OneShotTimer<AppListItemView> mouse_drag_timer_;

  DISALLOW_COPY_AND_ASSIGN(AppListItemView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_APP_LIST_ITEM_VIEW_H_
