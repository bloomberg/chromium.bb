// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_MESSAGE_CENTER_VIEWS_H_
#define UI_MESSAGE_CENTER_VIEWS_MESSAGE_CENTER_VIEWS_H_

#include "ui/views/view.h"

#include "ui/message_center/message_center_export.h"
#include "ui/message_center/notification_list.h"

namespace views {
class Button;
}  // namespace views

namespace message_center {

class MessageCenterBubble;
class MessageView;
class NotificationChangeObserver;

// MessageCenterButtonBar //////////////////////////////////////////////////////

// If you know how to better hide this implementation class please do so, and
// otherwise please refrain from using it :-).
class MessageCenterButtonBar : public views::View {
 public:
  explicit MessageCenterButtonBar(NotificationChangeObserver* observer);
  virtual ~MessageCenterButtonBar();

  void SetCloseAllVisible(bool visible);

 protected:
  NotificationChangeObserver* observer() { return observer_; }
  views::Button* close_all_button() { return close_all_button_; }
  void set_close_all_button(views::Button* button) {
    close_all_button_ = button;
  }

 private:
  NotificationChangeObserver* observer_;  // Weak reference.
  views::Button* close_all_button_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterButtonBar);
};

// MessageCenterView ///////////////////////////////////////////////////////////

class MESSAGE_CENTER_EXPORT MessageCenterView : public views::View {
 public:
  MessageCenterView(NotificationChangeObserver* observer, int max_height);
  virtual ~MessageCenterView();

  void SetNotifications(const NotificationList::Notifications& notifications);

  size_t NumMessageViewsForTest() const;

 protected:
  // Overridden from views::View:
  virtual void Layout() OVERRIDE;
  virtual bool OnMouseWheel(const ui::MouseWheelEvent& event) OVERRIDE;

 private:
  friend class MessageCenterViewTest;

  void RemoveAllNotifications();
  void AddNotification(const Notification& notification);

  NotificationChangeObserver* observer_;  // Weak reference.
  std::map<std::string,MessageView*> message_views_;
  views::ScrollView* scroller_;
  views::View* message_list_view_;
  MessageCenterButtonBar* button_bar_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterView);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_MESSAGE_CENTER_VIEWS_H_
