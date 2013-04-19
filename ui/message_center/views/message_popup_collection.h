// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_MESSAGE_POPUP_COLLECTION_H_
#define UI_MESSAGE_CENTER_VIEWS_MESSAGE_POPUP_COLLECTION_H_

#include <list>
#include <map>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"
#include "ui/message_center/message_center_export.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class Widget;
}

namespace ash {
FORWARD_DECLARE_TEST(WebNotificationTrayTest, ManyPopupNotifications);
}

namespace message_center {
namespace test {
class MessagePopupCollectionTest;
class MessagePopupCollectionWidgetsTest;
}

class MessageCenter;
class ToastContentsView;

// Container for popup toasts. Because each toast is a frameless window rather
// than a view in a bubble, now the container just manages all of those windows.
// This is similar to chrome/browser/notifications/balloon_collection, but the
// contents of each toast are for the message center and layout strategy would
// be slightly different.
class MESSAGE_CENTER_EXPORT MessagePopupCollection
    : public views::WidgetObserver,
      public MessageCenterObserver,
      public base::SupportsWeakPtr<MessagePopupCollection> {
 public:
  // |parent| specifies the parent widget of the toast windows. The default
  // parent will be used for NULL.
  MessagePopupCollection(gfx::NativeView parent,
                         MessageCenter* message_center);
  virtual ~MessagePopupCollection();

  void OnMouseEntered();
  void OnMouseExited();

 private:
  FRIEND_TEST_ALL_PREFIXES(ash::WebNotificationTrayTest,
                           ManyPopupNotifications);
  friend class test::MessagePopupCollectionTest;
  friend class test::MessagePopupCollectionWidgetsTest;
  typedef std::map<std::string, ToastContentsView*> ToastContainer;

  void CloseAllWidgets();

  // Returns the bottom-right position of the current work area.
  gfx::Point GetWorkAreaBottomRight();

  // Creates new widgets for new toast notifications, and updates |toasts_| and
  // |widgets_| correctly.
  void UpdateWidgets();

  // Repositions all of the widgets based on the current work area.
  void RepositionWidgets();

  // Repositions widgets. |target_rrect| denotes the region of the notification
  // recently removed, and this attempts to slide widgets so that the user can
  // click close-button without mouse moves. See crbug.com/224089
  void RepositionWidgetsWithTarget(const gfx::Rect& target_rect);

  // Overridden from views::WidgetObserver:
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE;

  // Overridden from MessageCenterObserver:
  virtual void OnNotificationAdded(const std::string& notification_id) OVERRIDE;
  virtual void OnNotificationRemoved(const std::string& notification_id,
                                     bool by_user) OVERRIDE;
  virtual void OnNotificationUpdated(
      const std::string& notification_id) OVERRIDE;

  void SetWorkAreaForTest(const gfx::Rect& work_area);
  views::Widget* GetWidgetForId(const std::string& id);

  gfx::NativeView parent_;
  MessageCenter* message_center_;
  ToastContainer toasts_;
  std::list<views::Widget*> widgets_;
  gfx::Rect work_area_;

  DISALLOW_COPY_AND_ASSIGN(MessagePopupCollection);
};

}  // namespace message_center

#endif // UI_MESSAGE_CENTER_VIEWS_MESSAGE_POPUP_COLLECTION_H_
