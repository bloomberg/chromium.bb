// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_BOUNDED_SCROLL_VIEW_H_
#define UI_MESSAGE_CENTER_VIEWS_BOUNDED_SCROLL_VIEW_H_

#include "ui/gfx/size.h"
#include "ui/message_center/message_center_export.h"
#include "ui/views/controls/scroll_view.h"

namespace message_center {

// A custom scroll view whose height has a minimum and maximum value and whose
// scroll bar disappears when not needed.
class MESSAGE_CENTER_EXPORT BoundedScrollView : public views::ScrollView {
 public:
  BoundedScrollView(int min_height, int max_height);

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual int GetHeightForWidth(int width) OVERRIDE;
  virtual void Layout() OVERRIDE;

 private:
  int min_height_;
  int max_height_;

  DISALLOW_COPY_AND_ASSIGN(BoundedScrollView);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_BOUNDED_SCROLL_VIEW_H_
