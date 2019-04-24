// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_MENU_BUTTON_EVENT_HANDLER_H_
#define UI_VIEWS_CONTROLS_BUTTON_MENU_BUTTON_EVENT_HANDLER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "ui/events/event.h"
#include "ui/views/controls/button/button.h"

namespace ui {
struct AXNodeData;
}
namespace views {
class MenuButton;
class MenuButtonListener;

// An EventHandler that contains the event behavior of a MenuButton.
// TODO (cyan): This will eventually be available to any Button derivative class
// that wants MenuButton behavior.
class VIEWS_EXPORT MenuButtonEventHandler : public ui::EventHandler {
 public:
  // A scoped lock for keeping the MenuButton in STATE_PRESSED e.g., while a
  // menu is running. These are cumulative.
  class VIEWS_EXPORT PressedLock {
   public:
    explicit PressedLock(MenuButtonEventHandler* menu_button_event_handler);

    // |event| is the event that caused the button to be pressed. May be null.
    PressedLock(MenuButtonEventHandler* menu_button_event_handler,
                bool is_sibling_menu_show,
                const ui::LocatedEvent* event);

    ~PressedLock();

   private:
    base::WeakPtr<MenuButtonEventHandler> menu_button_event_handler_;

    DISALLOW_COPY_AND_ASSIGN(PressedLock);
  };

  // TODO (cyan): MenuButtonEventHandler should take in a Button.
  MenuButtonEventHandler(MenuButton* menu_button_parent,
                         MenuButtonListener* menu_button_listener);
  ~MenuButtonEventHandler() override;

  // TODO (cyan): Methods in this block should override EventHandler methods.
  // These are currently called by the corresponding method from view::Views.
  bool OnMousePressed(const ui::MouseEvent& event);
  void OnMouseReleased(const ui::MouseEvent& event);
  void OnMouseEntered(const ui::MouseEvent& event);
  void OnMouseExited(const ui::MouseEvent& event);
  void OnMouseMoved(const ui::MouseEvent& event);
  bool OnKeyPressed(const ui::KeyEvent& event);
  bool OnKeyReleased(const ui::KeyEvent& event);
  void GetAccessibleNodeData(ui::AXNodeData* node_data);

  // ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Calls TakeLock with is_sibling_menu_show as false and a nullptr to the
  // event.
  std::unique_ptr<PressedLock> TakeLock();

  // Convenience method to increment the lock count on a button to keep the
  // button in a PRESSED state when a menu is showing.
  std::unique_ptr<PressedLock> TakeLock(bool is_sibling_menu_show,
                                        const ui::LocatedEvent* event);

  bool IsTriggerableEvent(const ui::Event& event);

  bool ShouldEnterPushedState(const ui::Event& event);
  void StateChanged(Button::ButtonState old_state);
  void NotifyClick(const ui::Event& event);

  // Activate the button (called when the button is pressed). |event| is the
  // event triggering the activation, if any.
  bool Activate(const ui::Event* event);

  // Returns true if the event is of the proper type to potentially trigger an
  // action. Since MenuButtons have properties other than event type (like
  // last menu open time) to determine if an event is valid to activate the
  // menu, this is distinct from IsTriggerableEvent().
  bool IsTriggerableEventType(const ui::Event& event);

  void OnMouseEvent(ui::MouseEvent* event) override;

 private:
  // Increment/decrement the number of "pressed" locks this button has, and
  // set the state accordingly. The ink drop is snapped to the final ACTIVATED
  // state if |snap_ink_drop_to_activated| is true, otherwise the ink drop
  // will be animated to the ACTIVATED node_data. The ink drop is animated at
  // the location of |event| if non-null, otherwise at the default location.
  void IncrementPressedLocked(bool snap_ink_drop_to_activated,
                              const ui::LocatedEvent* event);

  void DecrementPressedLocked();

  // Compute the maximum X coordinate for the current screen. MenuButtons
  // use this to make sure a menu is never shown off screen.
  int GetMaximumScreenXCoordinate();

  MenuButton* const menu_button_parent_;

  // Our listener. Not owned.
  MenuButtonListener* const listener_;

  // We use a time object in order to keep track of when the menu was closed.
  // The time is used for simulating menu behavior for the menu button; that
  // is, if the menu is shown and the button is pressed, we need to close the
  // menu. There is no clean way to get the second click event because the
  // menu is displayed using a modal loop and, unlike regular menus in
  // Windows, the button is not part of the displayed menu.
  base::TimeTicks menu_closed_time_;

  // The current number of "pressed" locks this button has.
  int pressed_lock_count_ = 0;

  // Used to let Activate() know if IncrementPressedLocked() was called.
  bool* increment_pressed_lock_called_ = nullptr;

  // True if the button was in a disabled state when a menu was run, and
  // should return to it once the press is complete. This can happen if, e.g.,
  // we programmatically show a menu on a disabled button.
  bool should_disable_after_press_ = false;

  base::WeakPtrFactory<MenuButtonEventHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MenuButtonEventHandler);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_MENU_BUTTON_EVENT_HANDLER_H_