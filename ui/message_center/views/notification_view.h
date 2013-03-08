// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_VIEW_H_
#define UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_VIEW_H_

#include <vector>

#include "ui/message_center/views/message_view.h"

namespace message_center {

class NotificationChangeObserver;

// View that displays all current types of notification (web, basic, image, and
// list). Future notification types may be handled by other classes, in which
// case instances of those classes would be returned by the Create() factory
// method below.
class NotificationView : public MessageView {
 public:
  // Creates appropriate MessageViews for notifications. Those currently are
  // always NotificationView or MessageSimpleView instances but in the future
  // may be instances of other classes, with the class depending on the
  // notification type.
  static MessageView* Create(const Notification& notification,
                             NotificationChangeObserver* observer,
                             bool expanded);

  virtual ~NotificationView();

  // Overridden from views::View:
  virtual void Layout() OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // Overridden from MessageView:
  virtual void Update(const Notification& notification) OVERRIDE;
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

 private:
  NotificationView(const Notification& notification,
                   NotificationChangeObserver* observer,
                   bool expanded);

  void AddChildViews(const Notification& notification);
  void AddContentView(const Notification& notification);

  views::View* content_view_;
  views::ImageView* icon_view_;
  std::vector<views::Button*> action_buttons_;

  DISALLOW_COPY_AND_ASSIGN(NotificationView);
};

}  // namespace message_center

#endif // UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_VIEW_H_
