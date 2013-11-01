// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_MESSAGE_CENTER_FOCUS_BORDER_H_
#define UI_MESSAGE_CENTER_VIEWS_MESSAGE_CENTER_FOCUS_BORDER_H_

#include "ui/views/focus_border.h"
#include "ui/views/view.h"

namespace message_center {

// Focus border class that draws a 1px blue border inset more on the bottom than
// on the sides.
class MessageCenterFocusBorder : public views::FocusBorder {
 public:
  MessageCenterFocusBorder();
  virtual ~MessageCenterFocusBorder();

 private:
  // views::FocusBorder overrides:
  virtual void Paint(const views::View& view, gfx::Canvas* canvas) const
      OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterFocusBorder);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_MESSAGE_CENTER_FOCUS_BORDER_H_
