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
#include "ui/message_center/message_center_export.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class Widget;
}

namespace ash {
FORWARD_DECLARE_TEST(WebNotificationTrayTest, ManyPopupNotifications);
}

namespace message_center {

class MessageCenter;
class ToastContentsView;

// Container for popup toasts. Because each toast is a frameless window rather
// than a view in a bubble, now the container just manages all of those windows.
// This is similar to chrome/browser/notifications/balloon_collection, but the
// contents of each toast are for the message center and layout strategy would
// be slightly different.
class MESSAGE_CENTER_EXPORT MessagePopupCollection
    : public views::WidgetObserver,
      public base::SupportsWeakPtr<MessagePopupCollection> {
 public:
  // |parent| specifies the parent widget of the toast windows. The default
  // parent will be used for NULL.
  MessagePopupCollection(gfx::NativeView parent,
                         MessageCenter* message_center);
  virtual ~MessagePopupCollection();

  void UpdatePopups();

  void OnMouseEntered();
  void OnMouseExited();

 private:
  FRIEND_TEST_ALL_PREFIXES(ash::WebNotificationTrayTest,
                           ManyPopupNotifications);
  typedef std::map<std::string, ToastContentsView*> ToastContainer;

  void CloseAllWidgets();

  // Overridden from views::WidgetObserver:
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE;

  gfx::NativeView parent_;
  MessageCenter* message_center_;
  ToastContainer toasts_;

  DISALLOW_COPY_AND_ASSIGN(MessagePopupCollection);
};

}  // namespace message_center

#endif // UI_MESSAGE_CENTER_VIEWS_MESSAGE_POPUP_COLLECTION_H_
