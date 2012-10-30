// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_MESSAGE_VIEW_H_
#define UI_MESSAGE_CENTER_MESSAGE_VIEW_H_

#include "ui/compositor/layer_animation_observer.h"
#include "ui/message_center/notification_list.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace views {
class ImageButton;
class ImageView;
class ScrollView;
}

namespace message_center {

// Individual notifications constants
const int kWebNotificationWidth = 320;
const int kWebNotificationButtonWidth = 32;
const int kWebNotificationIconSize = 40;

// The view for a notification entry (icon + message + buttons).
class MessageView : public views::View,
                    public views::ButtonListener,
                    public ui::ImplicitAnimationObserver {
 public:
  MessageView(NotificationList::Delegate* list_delegate,
              const NotificationList::Notification& notification,
              int scroll_bar_width);

  virtual ~MessageView();

  void set_scroller(views::ScrollView* scroller) { scroller_ = scroller; }

  // views::View overrides.
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;

  virtual ui::EventResult OnGestureEvent(
      const ui::GestureEvent& event) OVERRIDE;

  // Overridden from ButtonListener.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // Overridden from ImplicitAnimationObserver.
  virtual void OnImplicitAnimationsCompleted() OVERRIDE;

 private:
  enum SlideDirection {
    SLIDE_LEFT,
    SLIDE_RIGHT
  };

  // Shows the menu for the notification.
  void ShowMenu(gfx::Point screen_location);

  // Restores the transform and opacity of the view.
  void RestoreVisualState();

  // Slides the view out and closes it after the animation.
  void SlideOutAndClose(SlideDirection direction);

  NotificationList::Delegate* list_delegate_;
  NotificationList::Notification notification_;
  views::ImageView* icon_;
  views::ImageButton* close_button_;

  views::ScrollView* scroller_;
  float gesture_scroll_amount_;

  DISALLOW_COPY_AND_ASSIGN(MessageView);
};

}  // namespace message_center

#endif // UI_MESSAGE_CENTER_MESSAGE_VIEW_H_
