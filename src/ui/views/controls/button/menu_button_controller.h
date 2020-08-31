// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_MENU_BUTTON_CONTROLLER_H_
#define UI_VIEWS_CONTROLS_BUTTON_MENU_BUTTON_CONTROLLER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "ui/views/controls/button/button_controller.h"

namespace views {
class ButtonControllerDelegate;
class MenuButton;
class ButtonListener;

// A controller that contains the logic for showing a menu when the left mouse
// is pushed.
class VIEWS_EXPORT MenuButtonController : public ButtonController {
 public:
  // A scoped lock for keeping the MenuButton in STATE_PRESSED e.g., while a
  // menu is running. These are cumulative.
  class VIEWS_EXPORT PressedLock {
   public:
    explicit PressedLock(MenuButtonController* menu_button_controller);

    // |event| is the event that caused the button to be pressed. May be null.
    PressedLock(MenuButtonController* menu_button_controller,
                bool is_sibling_menu_show,
                const ui::LocatedEvent* event);

    ~PressedLock();

   private:
    base::WeakPtr<MenuButtonController> menu_button_controller_;

    DISALLOW_COPY_AND_ASSIGN(PressedLock);
  };

  MenuButtonController(Button* button,
                       ButtonListener* listener,
                       std::unique_ptr<ButtonControllerDelegate> delegate);
  ~MenuButtonController() override;

  // view::ButtonController
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseMoved(const ui::MouseEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool OnKeyReleased(const ui::KeyEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void UpdateAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnStateChanged(Button::ButtonState old_state) override;
  bool IsTriggerableEvent(const ui::Event& event) override;

  // Calls TakeLock with is_sibling_menu_show as false and a nullptr to the
  // event.
  std::unique_ptr<PressedLock> TakeLock();

  // Convenience method to increment the lock count on a button to keep the
  // button in a PRESSED state when a menu is showing.
  std::unique_ptr<PressedLock> TakeLock(bool is_sibling_menu_show,
                                        const ui::LocatedEvent* event);

  // Activate the button (called when the button is pressed). |event| is the
  // event triggering the activation, if any.
  bool Activate(const ui::Event* event);

  // Returns true if the event is of the proper type to potentially trigger an
  // action. Since MenuButtons have properties other than event type (like
  // last menu open time) to determine if an event is valid to activate the
  // menu, this is distinct from IsTriggerableEvent().
  bool IsTriggerableEventType(const ui::Event& event);

 private:
  // Increment/decrement the number of "pressed" locks this button has, and
  // set the state accordingly. The ink drop is snapped to the final ACTIVATED
  // state if |snap_ink_drop_to_activated| is true, otherwise the ink drop
  // will be animated to the ACTIVATED node_data. The ink drop is animated at
  // the location of |event| if non-null, otherwise at the default location.
  void IncrementPressedLocked(bool snap_ink_drop_to_activated,
                              const ui::LocatedEvent* event);

  void DecrementPressedLocked();

  // Our listener. Not owned.
  ButtonListener* const listener_;

  // We use a time object in order to keep track of when the menu was closed.
  // The time is used for simulating menu behavior for the menu button; that
  // is, if the menu is shown and the button is pressed, we need to close the
  // menu. There is no clean way to get the second click event because the
  // menu is displayed using a modal loop and, unlike regular menus in
  // Windows, the button is not part of the displayed menu.
  base::TimeTicks menu_closed_time_;

  // Tracks if the current triggering event should open a menu.
  bool is_intentional_menu_trigger_ = true;

  // The current number of "pressed" locks this button has.
  int pressed_lock_count_ = 0;

  // Used to let Activate() know if IncrementPressedLocked() was called.
  bool* increment_pressed_lock_called_ = nullptr;

  // True if the button was in a disabled state when a menu was run, and
  // should return to it once the press is complete. This can happen if, e.g.,
  // we programmatically show a menu on a disabled button.
  bool should_disable_after_press_ = false;

  base::WeakPtrFactory<MenuButtonController> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MenuButtonController);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_MENU_BUTTON_CONTROLLER_H_
