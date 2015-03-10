// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/test_views.h"

#include "ui/events/event.h"
#include "ui/views/widget/native_widget_private.h"
#include "ui/views/widget/widget.h"

namespace views {

StaticSizedView::StaticSizedView(const gfx::Size& size) : size_(size) {}

StaticSizedView::~StaticSizedView() {}

gfx::Size StaticSizedView::GetPreferredSize() const {
  return size_;
}

ProportionallySizedView::ProportionallySizedView(int factor)
    : factor_(factor), preferred_width_(-1) {}

ProportionallySizedView::~ProportionallySizedView() {}

int ProportionallySizedView::GetHeightForWidth(int w) const {
  return w * factor_;
}

gfx::Size ProportionallySizedView::GetPreferredSize() const {
  if (preferred_width_ >= 0)
    return gfx::Size(preferred_width_, GetHeightForWidth(preferred_width_));
  return View::GetPreferredSize();
}

CloseWidgetView::CloseWidgetView(ui::EventType event_type)
    : event_type_(event_type) {
}

void CloseWidgetView::OnEvent(ui::Event* event) {
  if (event->type() == event_type_) {
    // Go through NativeWidgetPrivate to simulate what happens if the OS
    // deletes the NativeWindow out from under us.
    // TODO(tapted): Change this to WidgetTest::SimulateNativeDestroy for a more
    // authentic test on Mac.
    GetWidget()->native_widget_private()->CloseNow();
  } else {
    View::OnEvent(event);
    if (!event->IsTouchEvent())
      event->SetHandled();
  }
}

}  // namespace views
