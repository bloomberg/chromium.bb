// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MESSAGE_CENTER_POPUPS_ONLY_UI_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_MESSAGE_CENTER_POPUPS_ONLY_UI_DELEGATE_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/notifications/popups_only_ui_controller.h"

namespace message_center {
class DesktopPopupAlignmentDelegate;
class MessagePopupCollection;
}  // namespace message_center

// A message center view implementation that shows notification popups (toasts)
// in the corner of the screen, but has no dedicated message center (widget with
// multiple notifications inside). This is used on Windows and Linux for
// non-native notifications.
class PopupsOnlyUiDelegate : public PopupsOnlyUiController::Delegate {
 public:
  PopupsOnlyUiDelegate();
  ~PopupsOnlyUiDelegate() override;

  // UiDelegate implementation.
  void ShowPopups() override;
  void HidePopups() override;

 private:
  std::unique_ptr<message_center::MessagePopupCollection> popup_collection_;
  std::unique_ptr<message_center::DesktopPopupAlignmentDelegate>
      alignment_delegate_;

  DISALLOW_COPY_AND_ASSIGN(PopupsOnlyUiDelegate);
};

#endif  // CHROME_BROWSER_UI_VIEWS_MESSAGE_CENTER_POPUPS_ONLY_UI_DELEGATE_H_
