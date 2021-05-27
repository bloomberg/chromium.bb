// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/native/menu_2.h"

#include "ui/base/models/menu_model.h"

namespace views {

Menu2::Menu2(ui::MenuModel* model)
    : model_(model), wrapper_(MenuWrapper::CreateWrapper(model)) {
  Rebuild();
}

Menu2::~Menu2() {}

HMENU Menu2::GetNativeMenu() const {
  return wrapper_->GetNativeMenu();
}

void Menu2::RunMenuAt(const gfx::Point& point, Alignment alignment) {
  wrapper_->RunMenuAt(point, alignment);
}

void Menu2::RunContextMenuAt(const gfx::Point& point) {
  RunMenuAt(point, ALIGN_TOPLEFT);
}

void Menu2::CancelMenu() {
  wrapper_->CancelMenu();
}

void Menu2::Rebuild() {
  wrapper_->Rebuild(nullptr);
}

void Menu2::UpdateStates() {
  wrapper_->UpdateStates();
}

MenuWrapper::MenuAction Menu2::GetMenuAction() const {
  return wrapper_->GetMenuAction();
}

void Menu2::SetMinimumWidth(int width) {
  wrapper_->SetMinimumWidth(width);
}

}  // namespace views
