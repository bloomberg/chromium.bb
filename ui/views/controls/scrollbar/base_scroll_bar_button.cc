// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/scrollbar/base_scroll_bar_button.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "ui/gfx/screen.h"

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
  // TODO(sky): See if we can convert to using |Screen| everywhere.
#if defined(OS_WIN) && !defined(USE_AURA)
  gfx::Point cursor_point(GetMessagePos());
#else
  gfx::Point cursor_point = gfx::Screen::GetCursorScreenPoint();
#endif
  views::MouseEvent event(ui::ET_MOUSE_RELEASED,
                          cursor_point.x(), cursor_point.y(),
                          ui::EF_LEFT_MOUSE_BUTTON);
  Button::NotifyClick(event);
}

}  // namespace views
