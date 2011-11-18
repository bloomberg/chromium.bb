// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/link.h"

#include "build/build_config.h"

#if defined(TOOLKIT_USES_GTK)
#include <gdk/gdk.h>
#endif

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font.h"
#include "ui/views/events/event.h"
#include "views/controls/link_listener.h"

#if defined(TOOLKIT_USES_GTK)
#include "ui/gfx/gtk_util.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/cursor.h"
#endif

namespace views {

const char Link::kViewClassName[] = "views/Link";

Link::Link() : Label(string16()) {
  Init();
}

Link::Link(const string16& title) : Label(title) {
  Init();
}

Link::~Link() {
}

void Link::OnEnabledChanged() {
  RecalculateFont();
  View::OnEnabledChanged();
}

std::string Link::GetClassName() const {
  return kViewClassName;
}

gfx::NativeCursor Link::GetCursor(const MouseEvent& event) {
  if (!IsEnabled())
    return gfx::kNullCursor;
#if defined(USE_AURA)
  return aura::kCursorHand;
#elif defined(OS_WIN)
  static HCURSOR g_hand_cursor = LoadCursor(NULL, IDC_HAND);
  return g_hand_cursor;
#elif defined(TOOLKIT_USES_GTK)
  return gfx::GetCursor(GDK_HAND2);
#endif
}

bool Link::HitTest(const gfx::Point& l) const {
  // We need to allow clicks on the link. So we override the implementation in
  // Label and use the default implementation of View.
  return View::HitTest(l);
}

bool Link::OnMousePressed(const MouseEvent& event) {
  if (!IsEnabled() ||
      (!event.IsLeftMouseButton() && !event.IsMiddleMouseButton()))
    return false;
  SetPressed(true);
  return true;
}

bool Link::OnMouseDragged(const MouseEvent& event) {
  SetPressed(IsEnabled() &&
             (event.IsLeftMouseButton() || event.IsMiddleMouseButton()) &&
             HitTest(event.location()));
  return true;
}

void Link::OnMouseReleased(const MouseEvent& event) {
  // Change the highlight first just in case this instance is deleted
  // while calling the controller
  OnMouseCaptureLost();
  if (IsEnabled() &&
      (event.IsLeftMouseButton() || event.IsMiddleMouseButton()) &&
      HitTest(event.location())) {
    // Focus the link on click.
    RequestFocus();

    if (listener_)
      listener_->LinkClicked(this, event.flags());
  }
}

void Link::OnMouseCaptureLost() {
  SetPressed(false);
}

bool Link::OnKeyPressed(const KeyEvent& event) {
  bool activate = ((event.key_code() == ui::VKEY_SPACE) ||
                   (event.key_code() == ui::VKEY_RETURN));
  if (!activate)
    return false;

  SetPressed(false);

  // Focus the link on key pressed.
  RequestFocus();

  if (listener_)
    listener_->LinkClicked(this, event.flags());

  return true;
}

bool Link::SkipDefaultKeyEventProcessing(const KeyEvent& event) {
  // Make sure we don't process space or enter as accelerators.
  return (event.key_code() == ui::VKEY_SPACE) ||
      (event.key_code() == ui::VKEY_RETURN);
}

void Link::GetAccessibleState(ui::AccessibleViewState* state) {
  Label::GetAccessibleState(state);
  state->role = ui::AccessibilityTypes::ROLE_LINK;
}

void Link::SetFont(const gfx::Font& font) {
  Label::SetFont(font);
  RecalculateFont();
}

void Link::SetEnabledColor(const SkColor& color) {
  requested_enabled_color_ = color;
  if (!pressed_)
    Label::SetEnabledColor(requested_enabled_color_);
}

void Link::SetPressedColor(const SkColor& color) {
  requested_pressed_color_ = color;
  if (pressed_)
    Label::SetEnabledColor(requested_pressed_color_);
}

void Link::Init() {
  static bool initialized = false;
  static SkColor kDefaultEnabledColor;
  static SkColor kDefaultDisabledColor;
  static SkColor kDefaultPressedColor;
  if (!initialized) {
#if defined(OS_WIN)
    kDefaultEnabledColor = color_utils::GetSysSkColor(COLOR_HOTLIGHT);
    kDefaultDisabledColor = color_utils::GetSysSkColor(COLOR_WINDOWTEXT);
    kDefaultPressedColor = SkColorSetRGB(200, 0, 0);
#else
    // TODO(beng): source from theme provider.
    kDefaultEnabledColor = SkColorSetRGB(0, 51, 153);
    kDefaultDisabledColor = SK_ColorBLACK;
    kDefaultPressedColor = SK_ColorRED;
#endif

    initialized = true;
  }

  listener_ = NULL;
  pressed_ = false;
  SetEnabledColor(kDefaultEnabledColor);
  SetDisabledColor(kDefaultDisabledColor);
  SetPressedColor(kDefaultPressedColor);
  RecalculateFont();
  set_focusable(true);
}

void Link::SetPressed(bool pressed) {
  if (pressed_ != pressed) {
    pressed_ = pressed;
    Label::SetEnabledColor(pressed_ ?
        requested_pressed_color_ : requested_enabled_color_);
    RecalculateFont();
    SchedulePaint();
  }
}

void Link::RecalculateFont() {
  // The font should be underlined iff the link is enabled.
  if (IsEnabled() == !(font().GetStyle() & gfx::Font::UNDERLINED)) {
    Label::SetFont(font().DeriveFont(0, IsEnabled() ?
        (font().GetStyle() | gfx::Font::UNDERLINED) :
        (font().GetStyle() & ~gfx::Font::UNDERLINED)));
  }
}

}  // namespace views
