// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_MESSAGE_CENTER_BUBBLE_H_
#define UI_MESSAGE_CENTER_VIEWS_MESSAGE_CENTER_BUBBLE_H_

#include <stddef.h>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/message_center/message_center_export.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class TrayBubbleView;
}

namespace message_center {

class MessageCenter;
class MessageCenterTray;
class MessageCenterView;

// Bubble for message center.
class MESSAGE_CENTER_EXPORT MessageCenterBubble
    : public views::WidgetObserver,
      public base::SupportsWeakPtr<MessageCenterBubble> {
 public:
  MessageCenterBubble(MessageCenter* message_center, MessageCenterTray* tray);
  ~MessageCenterBubble() override;

  // Gets called when the bubble view associated with this bubble is
  // destroyed. Clears |bubble_view_|.
  void BubbleViewDestroyed();

  // Sets/Gets the maximum height of the bubble view. Setting 0 changes the
  // bubble to the default size. max_height() will return the default size
  // if SetMaxHeight() has not been called yet.
  void SetMaxHeight(int height);
  int max_height() const { return max_height_; }

  void SetSettingsVisible();

  // Called after the bubble view has been constructed. Creates and initializes
  // the bubble contents.
  void InitializeContents(views::TrayBubbleView* bubble_view);

  bool IsVisible() const;

  // Overridden from views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override;

  size_t NumMessageViewsForTest() const;

  views::TrayBubbleView* bubble_view() const { return bubble_view_; }

 private:
  void UpdateBubbleView();

  MessageCenter* message_center_;
  MessageCenterTray* tray_;
  views::TrayBubbleView* bubble_view_ = nullptr;
  MessageCenterView* message_center_view_ = nullptr;

  // Use settings view as the initially visible content if true.
  bool initially_settings_visible_ = false;

  int max_height_;

  base::WeakPtrFactory<MessageCenterBubble> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MessageCenterBubble);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_MESSAGE_CENTER_BUBBLE_H_
