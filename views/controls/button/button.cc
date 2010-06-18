// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/button/button.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// Button, public:

Button::~Button() {
}

void Button::SetTooltipText(const std::wstring& tooltip_text) {
  tooltip_text_ = tooltip_text;
  TooltipTextChanged();
}

void Button::SetAccessibleKeyboardShortcut(const std::wstring& shortcut) {
  accessible_shortcut_.assign(shortcut);
}

////////////////////////////////////////////////////////////////////////////////
// Button, View overrides:

bool Button::GetTooltipText(const gfx::Point& p, std::wstring* tooltip) {
  if (tooltip_text_.empty())
    return false;

  *tooltip = tooltip_text_;
  return true;
}

bool Button::GetAccessibleKeyboardShortcut(std::wstring* shortcut) {
  if (!accessible_shortcut_.empty()) {
    *shortcut = accessible_shortcut_;
    return true;
  }
  return false;
}

bool Button::GetAccessibleRole(AccessibilityTypes::Role* role) {
  *role = AccessibilityTypes::ROLE_PUSHBUTTON;
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// Button, protected:

Button::Button(ButtonListener* listener)
    : listener_(listener),
      tag_(-1),
      mouse_event_flags_(0) {
  set_accessibility_focusable(true);
}

void Button::NotifyClick(const views::Event& event) {
  mouse_event_flags_ = event.IsMouseEvent() ? event.GetFlags() : 0;
  // We can be called when there is no listener, in cases like double clicks on
  // menu buttons etc.
  if (listener_)
    listener_->ButtonPressed(this, event);
  // NOTE: don't attempt to reset mouse_event_flags_ as the listener may have
  // deleted us.
}

}  // namespace views
