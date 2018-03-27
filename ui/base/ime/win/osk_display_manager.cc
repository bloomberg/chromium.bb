// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/win/osk_display_manager.h"

#include "base/debug/leak_annotations.h"
#include "base/win/windows_version.h"
#include "ui/base/ime/win/on_screen_keyboard_display_manager_stub.h"
#include "ui/base/ime/win/on_screen_keyboard_display_manager_tab_tip.h"

namespace ui {

// OnScreenKeyboardDisplayManager member definitions.
OnScreenKeyboardDisplayManager::OnScreenKeyboardDisplayManager() {}

OnScreenKeyboardDisplayManager::~OnScreenKeyboardDisplayManager() {}

OnScreenKeyboardDisplayManager* OnScreenKeyboardDisplayManager::GetInstance() {
  static OnScreenKeyboardDisplayManager* instance = nullptr;
  if (!instance) {
    if (base::win::GetVersion() < base::win::VERSION_WIN8)
      instance = new OnScreenKeyboardDisplayManagerStub();
    else
      instance = new OnScreenKeyboardDisplayManagerTabTip();
    ANNOTATE_LEAKING_OBJECT_PTR(instance);
  }
  return instance;
}

}  // namespace ui
