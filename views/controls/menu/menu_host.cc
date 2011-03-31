// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/menu/menu_host.h"
#include "views/controls/menu/native_menu_host.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// MenuHost, public:

MenuHost::MenuHost(SubmenuView* submenu)
    : native_menu_host_(NativeMenuHost::CreateNativeMenuHost(submenu)) {
}

MenuHost::~MenuHost() {
}

void MenuHost::InitMenuHost(gfx::NativeWindow parent,
                            const gfx::Rect& bounds,
                            View* contents_view,
                            bool do_capture) {
  native_menu_host_->InitMenuHost(parent, bounds, contents_view, do_capture);
}

bool MenuHost::IsMenuHostVisible() {
  return native_menu_host_->IsMenuHostVisible();
}

void MenuHost::ShowMenuHost(bool do_capture) {
  native_menu_host_->ShowMenuHost(do_capture);
}

void MenuHost::HideMenuHost() {
  native_menu_host_->HideMenuHost();
}

void MenuHost::DestroyMenuHost() {
  native_menu_host_->DestroyMenuHost();
  delete this;
}

void MenuHost::SetMenuHostBounds(const gfx::Rect& bounds) {
  native_menu_host_->SetMenuHostBounds(bounds);
}

void MenuHost::ReleaseMenuHostCapture() {
  native_menu_host_->ReleaseMenuHostCapture();
}

gfx::NativeWindow MenuHost::GetMenuHostWindow() {
  return native_menu_host_->GetMenuHostWindow();
}

}  // namespace views
