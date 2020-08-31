// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_APP_BUTTON_H_
#define ASH_SHELF_SHELF_APP_BUTTON_H_

#include "ash/ash_export.h"
#include "ash/shelf/shelf_button.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/gfx/shadow_value.h"
#include "ui/views/animation/ink_drop_observer.h"
#include "ui/views/animation/ink_drop_state.h"

namespace views {
class ImageView;
}

namespace ash {
struct ShelfItem;
class ShelfView;

// Button used for app shortcuts on the shelf..
class ASH_EXPORT ShelfAppButton : public ShelfButton,
                                  public views::InkDropObserver,
                                  public ui::ImplicitAnimationObserver {
 public:
  static const char kViewClassName[];

  // Used to indicate the current state of the button.
  enum State {
    // Nothing special. Usually represents an app shortcut item with no running
    // instance.
    STATE_NORMAL = 0,
    // Button has mouse hovering on it.
    STATE_HOVERED = 1 << 0,
    // Underlying ShelfItem has a running instance.
    STATE_RUNNING = 1 << 1,
    // Underlying ShelfItem needs user's attention.
    STATE_ATTENTION = 1 << 2,
    // Hide the status (temporarily for some animations).
    STATE_HIDDEN = 1 << 3,
    // Button is being dragged.
    STATE_DRAGGING = 1 << 4,
    // App has at least 1 notification.
    STATE_NOTIFICATION = 1 << 5,
    // Underlying ShelfItem owns the window that is currently active.
    STATE_ACTIVE = 1 << 6,
  };

  ShelfAppButton(ShelfView* shelf_view,
                 ShelfButtonDelegate* shelf_button_delegate);
  ~ShelfAppButton() override;

  // Sets the image to display for this entry.
  void SetImage(const gfx::ImageSkia& image);

  // Retrieve the image to show proxy operations.
  const gfx::ImageSkia& GetImage() const;

  // |state| is or'd into the current state.
  void AddState(State state);
  void ClearState(State state);
  int state() const { return state_; }

  // Returns the bounds of the icon.
  gfx::Rect GetIconBounds() const;

  // Returns the bounds of the icon in screen coordinates.
  gfx::Rect GetIconBoundsInScreen() const;

  views::InkDrop* GetInkDropForTesting();

  // Called when user started dragging the shelf button.
  void OnDragStarted(const ui::LocatedEvent* event);

  // Callback used when a menu for this ShelfAppButton is closed.
  void OnMenuClosed();

  // views::Button overrides:
  void ShowContextMenu(const gfx::Point& p,
                       ui::MenuSourceType source_type) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  bool ShouldEnterPushedState(const ui::Event& event) override;

  // views::View overrides:
  const char* GetClassName() const override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseCaptureLost() override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void Layout() override;
  void ChildPreferredSizeChanged(views::View* child) override;

  // Update button state from ShelfItem.
  void ReflectItemStatus(const ShelfItem& item);

  // Returns whether the icon size is up to date.
  bool IsIconSizeCurrent();

  bool FireDragTimerForTest();
  void FireRippleActivationTimerForTest();

  // Return the bounds in the local coordinates enclosing the small ripple area.
  gfx::Rect CalculateSmallRippleArea() const;

 protected:
  // ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override;

  // views::Button:
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

  // Sets the icon image with a shadow.
  void SetShadowedImage(const gfx::ImageSkia& bitmap);

 private:
  class AppNotificationIndicatorView;
  class AppStatusIndicatorView;

  // views::View:
  bool HandleAccessibleAction(const ui::AXActionData& action_data) override;

  // views::InkDropObserver:
  void InkDropAnimationStarted() override;
  void InkDropRippleAnimationEnded(views::InkDropState state) override;

  // Updates the parts of the button to reflect the current |state_| and
  // alignment. This may add or remove views, layout and paint.
  void UpdateState();

  // Invoked when |touch_drag_timer_| fires to show dragging UI.
  void OnTouchDragTimer();

  // Invoked when |ripple_activation_timer_| fires to activate the ink drop.
  void OnRippleTimer();

  // Scales up app icon if |scale_up| is true, otherwise scales it back to
  // normal size.
  void ScaleAppIcon(bool scale_up);

  // Calculates the icon bounds for an icon scaled by |icon_scale|.
  gfx::Rect GetIconViewBounds(float icon_scale);

  // Calculates the transform between the icon scaled by |icon_scale| and the
  // normal size icon.
  gfx::Transform GetScaleTransform(float icon_scale);

  // Marks whether the ink drop animation has started or not.
  void SetInkDropAnimationStarted(bool started);

  // The icon part of a button can be animated independently of the rest.
  views::ImageView* icon_view_;

  // The ShelfView showing this ShelfAppButton. Owned by RootWindowController.
  ShelfView* shelf_view_;

  // Draws an indicator underneath the image to represent the state of the
  // application.
  AppStatusIndicatorView* indicator_;

  // Draws an indicator in the top right corner of the image to represent an
  // active notification.
  AppNotificationIndicatorView* notification_indicator_;

  // The current application state, a bitfield of State enum values.
  int state_;

  gfx::ShadowValues icon_shadows_;

  // If non-null the destuctor sets this to true. This is set while the menu is
  // showing and used to detect if the menu was deleted while running.
  bool* destroyed_flag_;

  // Whether the notification indicator is enabled.
  const bool is_notification_indicator_enabled_;

  // The bitmap image for this app button.
  gfx::ImageSkia icon_image_;

  // The scaling factor for displaying the app icon.
  float icon_scale_ = 1.0f;

  // Indicates whether the ink drop animation starts.
  bool ink_drop_animation_started_ = false;

  // A timer to defer showing drag UI when the shelf button is pressed.
  base::OneShotTimer drag_timer_;

  // A timer to activate the ink drop ripple during a long press.
  base::OneShotTimer ripple_activation_timer_;

  DISALLOW_COPY_AND_ASSIGN(ShelfAppButton);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_APP_BUTTON_H_
