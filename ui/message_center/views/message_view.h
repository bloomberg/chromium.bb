// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_MESSAGE_VIEW_H_
#define UI_MESSAGE_CENTER_VIEWS_MESSAGE_VIEW_H_

#include "ui/gfx/insets.h"
#include "ui/message_center/notification.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/slide_out_view.h"

namespace views {
class ImageButton;
class ScrollView;
}

namespace message_center {

class NotificationChangeObserver;

// Individual notifications constants.
const int kPaddingBetweenItems = 10;
const int kPaddingHorizontal = 18;
const int kWebNotificationButtonWidth = 32;
const int kWebNotificationIconSize = 40;

// An abstract class that forms the basis of a view for a notification entry.
class MessageView : public views::SlideOutView,
                    public views::ButtonListener {
 public:
  MessageView(const Notification& notification,
              NotificationChangeObserver* observer,
              bool expanded);
  virtual ~MessageView();

  // Returns the insets for the shadow it will have for rich notification.
  static gfx::Insets GetShadowInsets();

  // Overridden from views::View:
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;

  // Overridden from ui::EventHandler:
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

  // Overridden from ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  void set_scroller(views::ScrollView* scroller) { scroller_ = scroller; }

 protected:
  MessageView();

  // Shows the menu for the notification.
  void ShowMenu(gfx::Point screen_location);

  // Overridden from views::SlideOutView:
  virtual void OnSlideOut() OVERRIDE;

  NotificationChangeObserver* observer() { return observer_; }
  const std::string& notification_id() { return notification_id_; }
  const string16& display_source() const { return display_source_; }
  const std::string& extension_id() const { return extension_id_; }
  views::ImageButton* close_button() { return close_button_.get(); }
  views::ImageButton* expand_button() { return expand_button_.get(); }
  views::ScrollView* scroller() { return scroller_; }
  const bool is_expanded() { return is_expanded_; }

 private:
  NotificationChangeObserver* observer_;  // Weak reference.
  std::string notification_id_;
  string16 display_source_;
  std::string extension_id_;

  scoped_ptr<views::ImageButton> close_button_;
  scoped_ptr<views::ImageButton> expand_button_;
  views::ScrollView* scroller_;

  bool is_expanded_;

  DISALLOW_COPY_AND_ASSIGN(MessageView);
};

}  // namespace message_center

#endif // UI_MESSAGE_CENTER_VIEWS_MESSAGE_VIEW_H_
