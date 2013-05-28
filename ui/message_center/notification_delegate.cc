// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/notification_delegate.h"

namespace message_center {

bool NotificationDelegate::HasClickedListener() { return false; }

void NotificationDelegate::ButtonClick(int button_index) {}

}  // namespace message_center
