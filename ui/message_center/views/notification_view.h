// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_VIEW_H_
#define UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_VIEW_H_

#include <vector>

#include "ui/message_center/views/message_view.h"

namespace views {
class Label;
}  // namespace views

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
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual int GetHeightForWidth(int width) OVERRIDE;
  virtual void Layout() OVERRIDE;

  // Overridden from MessageView:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

 private:
  NotificationView(const Notification& notification,
                   NotificationChangeObserver* observer,
                   bool expanded);

  // Truncate the very long text if we should. See crbug.com/222151 for the
  // details.
  string16 MaybeTruncateText(const string16& text, size_t limit);

  // Weak references to NotificationView descendants owned by their parents.
  views::View* background_view_;
  views::View* top_view_;
  views::Label* title_view_;
  views::Label* message_view_;
  std::vector<views::View*> item_views_;
  views::View* icon_view_;
  views::View* bottom_view_;
  views::View* image_view_;
  std::vector<views::View*> action_buttons_;

  DISALLOW_COPY_AND_ASSIGN(NotificationView);
};

}  // namespace message_center

#endif // UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_VIEW_H_
