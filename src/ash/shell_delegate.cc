// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell_delegate.h"

namespace ash {

bool ShellDelegate::AllowDefaultTouchActions(gfx::NativeWindow window) {
  return true;
}

bool ShellDelegate::ShouldWaitForTouchPressAck(gfx::NativeWindow window) {
  return false;
}

bool ShellDelegate::IsTabDrag(const ui::OSExchangeData& drop_data) {
  return false;
}

media_session::MediaSessionService* ShellDelegate::GetMediaSessionService() {
  return nullptr;
}

bool ShellDelegate::IsUiDevToolsStarted() const {
  return false;
}

int ShellDelegate::GetUiDevToolsPort() const {
  return -1;
}

desks_storage::DeskModel* ShellDelegate::GetDeskModel() {
  return nullptr;
}

}  // namespace ash
