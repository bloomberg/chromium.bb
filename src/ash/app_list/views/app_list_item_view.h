// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APP_LIST_ITEM_VIEW_H_
#define ASH_APP_LIST_VIEWS_APP_LIST_ITEM_VIEW_H_

#include <memory>
#include <string>
#include <utility>

#include "ash/app_list/model/app_icon_load_helper.h"
#include "ash/app_list/model/app_list_item_observer.h"
#include "ash/ash_export.h"
#include "base/memory/ref_counted.h"
#include "base/timer/timer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"

namespace gfx {
class Point;
class Rect;
}  // namespace gfx

namespace ui {
class LocatedEvent;
class SimpleMenuModel;
}  // namespace ui

namespace views {
class Label;
}  // namespace views

namespace ash {

class AppListConfig;
class AppListItem;
class AppListMenuModelAdapter;
class AppListViewDelegate;

namespace test {
class AppsGridViewTest;
class AppListMainViewTest;
}  // namespace test

// An application icon and title. Commonly part of the AppsGridView, but may be
// used in other contexts. Supports dragging and keyboard selection via the
// GridDelegate interface.
class ASH_EXPORT AppListItemView : public views::Button,
                                   public views::ContextMenuController,
                                   public AppListItemObserver,
                                   public ui::ImplicitAnimationObserver {
 public:
  METADATA_HEADER(AppListItemView);

  // The parent apps grid (AppsGridView) or a stub. Not named "Delegate" to
  // differentiate it from AppListViewDelegate.
  class GridDelegate {
   public:
    virtual ~GridDelegate() = default;

    // Whether the parent apps grid (if any) is a folder.
    virtual bool IsInFolder() const = 0;

    // Methods for keyboard selection.
    virtual void SetSelectedView(AppListItemView* view) = 0;
    virtual void ClearSelectedView() = 0;
    virtual bool IsSelectedView(const AppListItemView* view) const = 0;

    // Registers `view` as a dragged item with the apps grid. Called when the
    // user presses the mouse, or starts touch interaction with the view (both
    // of which may transition into a drag operation).
    // `location` - The pointer location in the view's bounds.
    // `root_location` - The pointer location in the root window coordinates.
    // `drag_start_callback` - Callback that gets called when the mouse/touch
    //     interaction transitions into a drag (i.e. when the "drag" item starts
    //     moving.
    //  `drag_end_callback` - Callback that gets called when drag interaction
    //     ends.
    //  Returns whether `view` has been registered as a dragged view. Callbacks
    //  should be ignored if the method returns false. If the method returns
    //  true, it's expected to eventually run `drag_end_callback`.
    virtual bool InitiateDrag(AppListItemView* view,
                              const gfx::Point& location,
                              const gfx::Point& root_location,
                              base::OnceClosure drag_start_callback,
                              base::OnceClosure drag_end_callback) = 0;
    virtual void StartDragAndDropHostDragAfterLongPress() = 0;
    // Called from AppListItemView when it receives a drag event. Returns true
    // if the drag is still happening.
    virtual bool UpdateDragFromItem(bool is_touch,
                                    const ui::LocatedEvent& event) = 0;
    virtual void EndDrag(bool cancel) = 0;

    // Provided as a callback for AppListItemView to notify of activation via
    // press/click/return key.
    virtual void OnAppListItemViewActivated(AppListItemView* pressed_item_view,
                                            const ui::Event& event) = 0;
  };

  AppListItemView(const AppListConfig* app_list_config,
                  GridDelegate* grid_delegate,
                  AppListItem* item,
                  AppListViewDelegate* view_delegate);
  AppListItemView(const AppListItemView&) = delete;
  AppListItemView& operator=(const AppListItemView&) = delete;
  ~AppListItemView() override;

  // Sets the app list config that should be used to size the app list icon, and
  // margins within the app list item view. The owner should ensure the
  // `AppListItemView` does not outlive the object referenced by
  // `app_list_config_`.
  void UpdateAppListConfig(const AppListConfig* app_list_config);

  // Sets the icon of this image.
  void SetIcon(const gfx::ImageSkia& icon);

  void SetItemName(const std::u16string& display_name,
                   const std::u16string& full_name);

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  void CancelContextMenu();

  gfx::Point GetDragImageOffset();

  void SetAsAttemptedFolderTarget(bool is_target_folder);

  // Sets focus without a11y announcements or focus ring.
  void SilentlyRequestFocus();

  AppListItem* item() const { return item_weak_; }

  views::Label* title() { return title_; }

  // In a synchronous drag the item view isn't informed directly of the drag
  // ending, so the runner of the drag should call this.
  void OnSyncDragEnd();

  // Returns the icon bounds relative to AppListItemView.
  gfx::Rect GetIconBounds() const;

  // Returns the icon bounds in screen.
  gfx::Rect GetIconBoundsInScreen() const;

  // Returns the image of icon.
  gfx::ImageSkia GetIconImage() const;

  // Sets the icon's visibility.
  void SetIconVisible(bool visible);

  // Handles the icon's scaling and animation for a cardified grid.
  void EnterCardifyState();
  void ExitCardifyState();

  // Returns the icon bounds for with |target_bounds| as the bounds of this view
  // and given |icon_size| and the |icon_scale| if the icon was scaled from the
  // original display size.
  static gfx::Rect GetIconBoundsForTargetViewBounds(
      const AppListConfig* config,
      const gfx::Rect& target_bounds,
      const gfx::Size& icon_size,
      float icon_scale);

  // Returns the title bounds for with |target_bounds| as the bounds of this
  // view and given |title_size| and the |icon_scale| if the icon was scaled
  // from the original display size.
  static gfx::Rect GetTitleBoundsForTargetViewBounds(
      const AppListConfig* config,
      const gfx::Rect& target_bounds,
      const gfx::Size& title_size,
      float icon_scale);

  // views::Button overrides:
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnThemeChanged() override;

  // views::View overrides:
  std::u16string GetTooltipText(const gfx::Point& p) const override;

  // When a dragged view enters this view, a preview circle is shown for
  // non-folder item while the icon is enlarged for folder item. When a
  // dragged view exits this view, the reverse animation will be performed.
  void OnDraggedViewEnter();
  void OnDraggedViewExit();

  // Enables background blur for folder icon if |enabled| is true.
  void SetBackgroundBlurEnabled(bool enabled);

  // Ensures this item view has its own layer.
  void EnsureLayer();

  bool HasNotificationBadge();

  void FireMouseDragTimerForTest();

  bool FireTouchDragTimerForTest();

  bool is_folder() const { return is_folder_; }

  bool IsNotificationIndicatorShownForTest() const;
  GridDelegate* grid_delegate_for_test() { return grid_delegate_; }

 private:
  friend class test::AppsGridViewTest;
  friend class test::AppListMainViewTest;

  class IconImageView;
  class AppNotificationIndicatorView;

  enum UIState {
    UI_STATE_NORMAL,              // Normal UI (icon + label)
    UI_STATE_DRAGGING,            // Dragging UI (scaled icon only)
    UI_STATE_DROPPING_IN_FOLDER,  // Folder dropping preview UI
  };

  // Describes the app list item view drag state.
  enum class DragState {
    // Item is not being dragged.
    kNone,

    // Drag is initialized for the item (the owning apps grid considers the view
    // to be the dragged view), but the item is still not being dragged.
    // Depending on mouse/touch drag timers, UI may be in either normal, or
    // dragging state.
    kInitialized,

    // The item drag is in progress. While in this state, the owning apps grid
    // view will generally hide the item view, and replace it with a drag icon
    // widget. The UI should be in dragging state (scaled up and with title
    // hidden).
    kStarted,
  };

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;

  // Callback used when a menu is closed.
  void OnMenuClosed();

  // Get icon from |item_| and schedule background processing.
  void UpdateIcon();

  // Update the tooltip text from |item_|.
  void UpdateTooltip();

  void SetUIState(UIState state);

  // Scales up app icon if |scale_up| is true; otherwise, scale it back to
  // normal size.
  void ScaleAppIcon(bool scale_up);

  // Scale app icon to |scale_factor| without animation.
  void ScaleIconImmediatly(float scale_factor);

  // Sets |touch_dragging_| flag and updates UI.
  void SetTouchDragging(bool touch_dragging);
  // Sets |mouse_dragging_| flag and updates UI. Only to be called on
  // |mouse_drag_timer_|.
  void SetMouseDragging(bool mouse_dragging);

  // Invoked when |mouse_drag_timer_| fires to show dragging UI.
  void OnMouseDragTimer();

  // Invoked when |touch_drag_timer_| fires to show dragging UI.
  void OnTouchDragTimer(const gfx::Point& tap_down_location,
                        const gfx::Point& tap_down_root_location);

  // Registers this view as a dragged view with the grid delegate.
  bool InitiateDrag(const gfx::Point& location,
                    const gfx::Point& root_location);

  // Called when the drag registered for this view starts moving.
  // `drag_start_callback` passed to `GridDelegate::InitiateDrag()`.
  void OnDragStarted();

  // Called when the drag registered for this view ends.
  // `drag_end_callback` passed to `GridDelegate::InitiateDrag()`.
  void OnDragEnded();

  // Callback invoked when a context menu is received after calling
  // |AppListViewDelegate::GetContextMenuModel|.
  void OnContextMenuModelReceived(
      const gfx::Point& point,
      ui::MenuSourceType source_type,
      std::unique_ptr<ui::SimpleMenuModel> menu_model);

  // views::ContextMenuController overrides:
  void ShowContextMenuForViewImpl(views::View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

  // views::Button overrides:
  bool ShouldEnterPushedState(const ui::Event& event) override;
  void PaintButtonContents(gfx::Canvas* canvas) override;

  // views::View overrides:
  void Layout() override;
  gfx::Size CalculatePreferredSize() const override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseCaptureLost() override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  bool SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) override;
  void OnFocus() override;
  void OnBlur() override;

  // AppListItemObserver overrides:
  void ItemIconChanged(AppListConfigType config_type) override;
  void ItemNameChanged() override;
  void ItemBadgeVisibilityChanged() override;
  void ItemBadgeColorChanged() override;
  void ItemBeingDestroyed() override;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

  // Returns the radius of preview circle.
  int GetPreviewCircleRadius() const;

  // Creates dragged view hover animation if it does not exist.
  void CreateDraggedViewHoverAnimation();

  // Modifies AppListItemView bounds to match the selected highlight bounds.
  void AdaptBoundsForSelectionHighlight(gfx::Rect* rect);

  // Calculates the transform between the icon scaled by |icon_scale| and the
  // normal size icon.
  gfx::Transform GetScaleTransform(float icon_scale);

  // The app list config used to layout this view. The initial values is set
  // during view construction, but can be changed by calling
  // `UpdateAppListConfig()`.
  const AppListConfig* app_list_config_;

  const bool is_folder_;

  // Whether context menu options have been requested. Prevents multiple
  // requests.
  bool waiting_for_context_menu_options_ = false;

  AppListItem* item_weak_;  // Owned by AppListModel. Can be nullptr.

  // Handles dragging and item selection. Might be a stub for items that are not
  // part of an apps grid.
  GridDelegate* const grid_delegate_;

  // AppListControllerImpl by another name.
  AppListViewDelegate* const view_delegate_;

  IconImageView* icon_ = nullptr;               // Strongly typed child view.
  views::Label* title_ = nullptr;               // Strongly typed child view.

  std::unique_ptr<AppListMenuModelAdapter> context_menu_;

  UIState ui_state_ = UI_STATE_NORMAL;

  // True if scroll gestures should contribute to dragging.
  bool touch_dragging_ = false;

  // True if the app is enabled for drag/drop operation by mouse.
  bool mouse_dragging_ = false;

  // Whether AppsGridView should not be notified of a focus event, triggering
  // A11y alerts and a focus ring.
  bool focus_silently_ = false;

  // Whether AppsGridView is in cardified state.
  bool in_cardified_grid_ = false;

  // The animation that runs when dragged view enters or exits this view.
  std::unique_ptr<gfx::SlideAnimation> dragged_view_hover_animation_;

  // The radius of preview circle for non-folder item.
  int preview_circle_radius_ = 0;

  // Whether |context_menu_| was cancelled as the result of a continuous drag
  // gesture.
  bool menu_close_initiated_from_drag_ = false;

  // Whether |context_menu_| was shown via key event.
  bool menu_show_initiated_from_key_ = false;

  std::u16string tooltip_text_;

  // A timer to defer showing drag UI when mouse is pressed.
  base::OneShotTimer mouse_drag_timer_;
  // A timer to defer showing drag UI when the app item is touch pressed.
  base::OneShotTimer touch_drag_timer_;

  // The shadow margins added to the app list item title.
  gfx::Insets title_shadow_margins_;

  // The bitmap image for this app list item.
  gfx::ImageSkia icon_image_;

  // The current item's drag state.
  DragState drag_state_ = DragState::kNone;

  // The scaling factor for displaying the app icon.
  float icon_scale_ = 1.0f;

  // Draws an indicator in the top right corner of the image to represent an
  // active notification.
  AppNotificationIndicatorView* notification_indicator_ = nullptr;

  // Helper to trigger icon load.
  absl::optional<AppIconLoadHelper> icon_load_helper_;

  base::WeakPtrFactory<AppListItemView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APP_LIST_ITEM_VIEW_H_
