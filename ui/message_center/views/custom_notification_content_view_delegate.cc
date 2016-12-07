// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/custom_notification_content_view_delegate.h"

#include "ui/views/view.h"

namespace message_center {

CustomContent::CustomContent(
    std::unique_ptr<views::View> view,
    std::unique_ptr<CustomNotificationContentViewDelegate> delegate)
    : view(std::move(view)), delegate(std::move(delegate)) {}

CustomContent::~CustomContent() {}

}  // namespace message_center
