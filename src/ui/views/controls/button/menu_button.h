// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_MENU_BUTTON_H_
#define UI_VIEWS_CONTROLS_BUTTON_MENU_BUTTON_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/menu_button_event_handler.h"

namespace views {

class MenuButtonListener;

////////////////////////////////////////////////////////////////////////////////
//
// MenuButton
//
//  A button that shows a menu when the left mouse button is pushed
//
////////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT MenuButton : public LabelButton {
 public:
  static const char kViewClassName[];

  // How much padding to put on the left and right of the menu marker.
  static const int kMenuMarkerPaddingLeft;
  static const int kMenuMarkerPaddingRight;

  // Create a Button.
  MenuButton(const base::string16& text,
             MenuButtonListener* menu_button_listener);
  ~MenuButton() override;

  bool Activate(const ui::Event* event);

  // TODO(cyan): Move into MenuButtonEventHandler.
  virtual bool IsTriggerableEventType(const ui::Event& event);

  // View:
  const char* GetClassName() const override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool OnKeyReleased(const ui::KeyEvent& event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Disallow overriding of these methods.
  // No-op implementations as these are handled in MenuButtonEventHandler (and
  // should not be handled here as well). To override these you need to subclass
  // MenuButtonEventHandler and replace this event handler of the button.
  void OnMouseEntered(const ui::MouseEvent& event) final;
  void OnMouseExited(const ui::MouseEvent& event) final;
  void OnMouseMoved(const ui::MouseEvent& event) final;

  // Protected methods needed for MenuButtonEventHandler.
  // TODO (cyan): Move these to a delegate interface.
  using InkDropHostView::GetInkDrop;
  using View::InDrag;
  using View::GetDragOperations;
  using Button::ShouldEnterHoveredState;
  void LabelButtonStateChanged(ButtonState old_state);

  // Button:
  bool IsTriggerableEvent(const ui::Event& event) override;

  MenuButtonEventHandler* menu_button_event_handler() {
    return &menu_button_event_handler_;
  }

 protected:
  void StateChanged(ButtonState old_state) override;

  // Button:
  bool ShouldEnterPushedState(const ui::Event& event) final;
  void NotifyClick(const ui::Event& event) final;

 private:
  // All events get sent to this handler to be processed.
  // TODO (cyan): This will be generalized into a ButtonEventHandler and moved
  // into the Button class.
  MenuButtonEventHandler menu_button_event_handler_;

  DISALLOW_COPY_AND_ASSIGN(MenuButton);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_MENU_BUTTON_H_
