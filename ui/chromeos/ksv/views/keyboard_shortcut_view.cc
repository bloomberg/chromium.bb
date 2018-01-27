// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/chromeos/ksv/views/keyboard_shortcut_view.h"

#include "ui/views/background.h"
#include "ui/views/widget/widget.h"

namespace keyboard_shortcut_viewer {

namespace {

KeyboardShortcutView* g_ksv_view = nullptr;

}  // namespace

KeyboardShortcutView::~KeyboardShortcutView() {
  DCHECK_EQ(g_ksv_view, this);
  g_ksv_view = nullptr;
}

// static
views::Widget* KeyboardShortcutView::Show(gfx::NativeWindow context) {
  if (g_ksv_view) {
    // If there is a KeyboardShortcutView window open already, just activate it.
    g_ksv_view->GetWidget()->Activate();
  } else {
    // TODO(wutao): change the bounds to UX specs when they are available.
    views::Widget::CreateWindowWithContextAndBounds(
        new KeyboardShortcutView(), context, gfx::Rect(0, 0, 660, 560));
    g_ksv_view->GetWidget()->Show();
  }
  return g_ksv_view->GetWidget();
}

KeyboardShortcutView::KeyboardShortcutView() {
  DCHECK_EQ(g_ksv_view, nullptr);
  g_ksv_view = this;

  // Default background is transparent.
  SetBackground(views::CreateSolidBackground(SK_ColorWHITE));
  InitViews();
}

void KeyboardShortcutView::InitViews() {
  // TODO(wutao): implement convertion from KeyboardShortcutItem to
  // KeyboardShortcutItemView, including generating strings and icons for VKEY.
  // Call GetKeyboardShortcutItemList() to constuct the item views.
  NOTIMPLEMENTED();
}

bool KeyboardShortcutView::CanMaximize() const {
  return false;
}

bool KeyboardShortcutView::CanMinimize() const {
  return true;
}

bool KeyboardShortcutView::CanResize() const {
  return false;
}

views::ClientView* KeyboardShortcutView::CreateClientView(
    views::Widget* widget) {
  return new views::ClientView(widget, this);
}

}  // namespace keyboard_shortcut_viewer
