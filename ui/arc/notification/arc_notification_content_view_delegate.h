// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ARC_NOTIFICATION_ARC_NOTIFICATION_CONTENT_VIEW_DELEGATE_H_
#define UI_ARC_NOTIFICATION_ARC_NOTIFICATION_CONTENT_VIEW_DELEGATE_H_

namespace message_center {
class NotificationControlButtonsView;
}

namespace arc {

// Delegate for a view that is hosted by CustomNotificationView.
// TODO(yoshiki): Remove this delegate and call code in the content view
// directly without delegate.
class ArcNotificationContentViewDelegate {
 public:
  virtual bool IsCloseButtonFocused() const = 0;
  virtual void RequestFocusOnCloseButton() = 0;
  virtual void UpdateControlButtonsVisibility() = 0;
  virtual void OnSlideChanged() = 0;
  virtual message_center::NotificationControlButtonsView*
  GetControlButtonsView() const = 0;
};

}  // namespace arc

#endif  // UI_ARC_NOTIFICATION_ARC_NOTIFICATION_CONTENT_VIEW_DELEGATE_H_
