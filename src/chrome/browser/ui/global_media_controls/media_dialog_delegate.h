// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_DIALOG_DELEGATE_H_

#include <string>

#include "base/memory/weak_ptr.h"

namespace media_message_center {
class MediaNotificationItem;
}  // namespace media_message_center

// Delegate for MediaToolbarButtonController that is told when to display or
// hide a media session.
class MediaDialogDelegate {
 public:
  virtual void ShowMediaSession(
      const std::string& id,
      base::WeakPtr<media_message_center::MediaNotificationItem> item) = 0;
  virtual void HideMediaSession(const std::string& id) = 0;

 protected:
  virtual ~MediaDialogDelegate();
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_DIALOG_DELEGATE_H_
