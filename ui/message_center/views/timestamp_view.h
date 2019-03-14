// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_TIMESTAMP_VIEW_H_
#define UI_MESSAGE_CENTER_VIEWS_TIMESTAMP_VIEW_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "ui/message_center/message_center_export.h"
#include "ui/views/controls/label.h"

namespace message_center {

class MESSAGE_CENTER_EXPORT TimestampView : public views::Label {
 public:
  TimestampView();
  ~TimestampView() override;

  // Formats |timestamp| relative to base::Time::Now().
  void SetTimestamp(base::Time timestamp);

 private:
  DISALLOW_COPY_AND_ASSIGN(TimestampView);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_TIMESTAMP_VIEW_H_
