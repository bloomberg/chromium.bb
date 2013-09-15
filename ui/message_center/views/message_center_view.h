// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_MESSAGE_CENTER_VIEW_H_
#define UI_MESSAGE_CENTER_VIEWS_MESSAGE_CENTER_VIEW_H_

#include "ui/views/view.h"

#include "ui/gfx/animation/animation_delegate.h"
#include "ui/message_center/message_center_export.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/notification_list.h"
#include "ui/views/controls/button/button.h"

namespace gfx {
class MultiAnimation;
}  // namespace gfx

namespace views {
class Button;
}  // namespace views

namespace message_center {

class MessageCenter;
class MessageCenterBubble;
class NotificationCenterButton;
class MessageCenterButtonBar;
class MessageCenterTray;
class MessageCenterView;
class MessageView;
class MessageListView;
class NotifierSettingsView;

// MessageCenterView ///////////////////////////////////////////////////////////

class MESSAGE_CENTER_EXPORT MessageCenterView : public views::View,
                                                public MessageCenterObserver,
                                                public gfx::AnimationDelegate {
 public:
  MessageCenterView(MessageCenter* message_center,
                    MessageCenterTray* tray,
                    int max_height,
                    bool initially_settings_visible,
                    bool top_down);
  virtual ~MessageCenterView();

  void SetNotifications(const NotificationList::Notifications& notifications);

  void ClearAllNotifications();
  void OnAllNotificationsCleared();

  size_t NumMessageViewsForTest() const;

  void SetSettingsVisible(bool visible);
  void OnSettingsChanged();
  bool settings_visible() const { return settings_visible_; }

  void SetIsClosing(bool is_closing);

 protected:
  // Overridden from views::View:
  virtual void Layout() OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual int GetHeightForWidth(int width) OVERRIDE;
  virtual bool OnMouseWheel(const ui::MouseWheelEvent& event) OVERRIDE;
  virtual void OnMouseExited(const ui::MouseEvent& event) OVERRIDE;

  // Overridden from MessageCenterObserver:
  virtual void OnNotificationAdded(const std::string& id) OVERRIDE;
  virtual void OnNotificationRemoved(const std::string& id,
                                     bool by_user) OVERRIDE;
  virtual void OnNotificationUpdated(const std::string& id) OVERRIDE;

  // Overridden from gfx::AnimationDelegate:
  virtual void AnimationEnded(const gfx::Animation* animation) OVERRIDE;
  virtual void AnimationProgressed(const gfx::Animation* animation) OVERRIDE;
  virtual void AnimationCanceled(const gfx::Animation* animation) OVERRIDE;

 private:
  friend class MessageCenterViewTest;

  void AddNotificationAt(const Notification& notification, int index);
  void NotificationsChanged();
  void SetNotificationViewForTest(views::View* view);

  MessageCenter* message_center_;  // Weak reference.
  MessageCenterTray* tray_;  // Weak reference.
  std::vector<MessageView*> message_views_;  // Weak references.

  // Child views.
  views::ScrollView* scroller_;
  MessageListView* message_list_view_;
  NotifierSettingsView* settings_view_;
  MessageCenterButtonBar* button_bar_;
  views::View* no_notifications_message_view_;
  bool top_down_;

  // Data for transition animation between settings view and message list.
  bool settings_visible_;

  // Animation managing transition between message center and settings (and vice
  // versa).
  scoped_ptr<gfx::MultiAnimation> settings_transition_animation_;

  // Helper data to keep track of the transition between settings and
  // message center views.
  views::View* source_view_;
  int source_height_;
  views::View* target_view_;
  int target_height_;

  // True when the widget is closing so that further operations should be
  // ignored.
  bool is_closing_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterView);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_MESSAGE_CENTER_VIEW_H_
