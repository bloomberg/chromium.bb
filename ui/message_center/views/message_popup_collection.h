// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_MESSAGE_POPUP_COLLECTION_H_
#define UI_MESSAGE_CENTER_VIEWS_MESSAGE_POPUP_COLLECTION_H_

#include <list>
#include <map>

#include "base/timer.h"
#include "ui/message_center/message_center_export.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/views/message_bubble_base.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class Widget;
}

namespace message_center {

class ToastContentsView;

// Container for popup toasts. Because each toast is a frameless window rather
// than a view in a bubble, now the container just manages all of those windows.
// This is similar to chrome/browser/notifications/balloon_collection, but the
// contents of each toast are for the message center and layout strategy would
// be slightly different.
class MESSAGE_CENTER_EXPORT MessagePopupCollection
    : public views::WidgetObserver {
 public:
  // |context| specifies the context to create toast windows. It can be NULL
  // for non-aura environment. See comments in ui/views/widget/widget.h.
  MessagePopupCollection(gfx::NativeView context,
                         NotificationList::Delegate* list_delegate);
  virtual ~MessagePopupCollection();

  void UpdatePopups();

  void OnMouseEntered();
  void OnMouseExited();

 private:
  typedef std::map<std::string, ToastContentsView*> ToastContainer;

  void CloseAllWidgets();

  // views::WidgetObserver overrides:
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE;

  gfx::NativeView context_;
  NotificationList::Delegate* list_delegate_;
  ToastContainer toasts_;

  DISALLOW_COPY_AND_ASSIGN(MessagePopupCollection);
};

}  // namespace message_center

#endif // UI_MESSAGE_CENTER_VIEWS_MESSAGE_POPUP_COLLECTION_H_
