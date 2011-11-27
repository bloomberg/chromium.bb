// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/scrollbar/base_scroll_bar_button.h"

#include "base/bind.h"
#include "base/bind_helpers.h"

namespace views {

BaseScrollBarButton::BaseScrollBarButton(ButtonListener* listener)
    : CustomButton(listener),
      ALLOW_THIS_IN_INITIALIZER_LIST(repeater_(
          base::Bind(&BaseScrollBarButton::RepeaterNotifyClick,
                     base::Unretained(this)))) {
}

BaseScrollBarButton::~BaseScrollBarButton() {
}

bool BaseScrollBarButton::OnMousePressed(const MouseEvent& event) {
  Button::NotifyClick(event);
  repeater_.Start();
  return true;
}

void BaseScrollBarButton::OnMouseReleased(const MouseEvent& event) {
  OnMouseCaptureLost();
}

void BaseScrollBarButton::OnMouseCaptureLost() {
  repeater_.Stop();
}

void BaseScrollBarButton::RepeaterNotifyClick() {
#if defined(OS_WIN)
  DWORD pos = GetMessagePos();
  POINTS points = MAKEPOINTS(pos);
  gfx::Point cursor_point(points.x, points.y);
#elif defined(OS_LINUX)
  gfx::Point cursor_point = gfx::Screen::GetCursorScreenPoint();
#endif
  views::MouseEvent event(ui::ET_MOUSE_RELEASED,
                          cursor_point.x(), cursor_point.y(),
                          ui::EF_LEFT_BUTTON_DOWN);
  Button::NotifyClick(event);
}

}  // namespace views
