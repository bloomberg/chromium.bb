// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_MESSAGE_VIEW_H_
#define UI_MESSAGE_CENTER_VIEWS_MESSAGE_VIEW_H_

#include "base/strings/string16.h"
#include "ui/gfx/insets.h"
#include "ui/message_center/message_center_export.h"
#include "ui/message_center/notification.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/slide_out_view.h"

namespace views {
class ImageButton;
class ScrollView;
}

namespace message_center {

class MessageCenter;
class MessageCenterTray;
class MessageViewContextMenuController;

// Individual notifications constants.
const int kPaddingBetweenItems = 10;
const int kPaddingHorizontal = 18;
const int kWebNotificationButtonWidth = 32;
const int kWebNotificationIconSize = 40;

// An abstract class that forms the basis of a view for a notification entry.
class MESSAGE_CENTER_EXPORT MessageView : public views::SlideOutView,
                                          public views::ButtonListener {
 public:
  MessageView(const Notification& notification,
              MessageCenter* message_center,
              MessageCenterTray* tray,
              bool expanded);
  virtual ~MessageView();

  // Returns the insets for the shadow it will have for rich notification.
  static gfx::Insets GetShadowInsets();

  // Creates a shadow around the notification.
  void CreateShadowBorder();

  bool IsCloseButtonFocused();
  void RequestFocusOnCloseButton();

  void set_accessible_name(const string16& name) { accessible_name_ = name; }

  // Overridden from views::View:
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;
  virtual bool OnKeyPressed(const ui::KeyEvent& event) OVERRIDE;
  virtual bool OnKeyReleased(const ui::KeyEvent& event) OVERRIDE;
  virtual void OnPaintFocusBorder(gfx::Canvas* canvas) OVERRIDE;

  // Overridden from ui::EventHandler:
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

  // Overridden from ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  const std::string& notification_id() { return notification_id_; }
  void set_scroller(views::ScrollView* scroller) { scroller_ = scroller; }

 protected:
  MessageView();

  // Overridden from views::SlideOutView:
  virtual void OnSlideOut() OVERRIDE;

  MessageCenter* message_center() { return message_center_; }
  views::ImageButton* close_button() { return close_button_.get(); }
  views::ImageButton* expand_button() { return expand_button_.get(); }
  views::ScrollView* scroller() { return scroller_; }
  const bool is_expanded() { return is_expanded_; }

 private:
  MessageCenter* message_center_;  // Weak reference.
  std::string notification_id_;

  scoped_ptr<MessageViewContextMenuController> context_menu_controller_;
  scoped_ptr<views::ImageButton> close_button_;
  scoped_ptr<views::ImageButton> expand_button_;
  views::ScrollView* scroller_;

  string16 accessible_name_;

  bool is_expanded_;

  DISALLOW_COPY_AND_ASSIGN(MessageView);
};

}  // namespace message_center

#endif // UI_MESSAGE_CENTER_VIEWS_MESSAGE_VIEW_H_
