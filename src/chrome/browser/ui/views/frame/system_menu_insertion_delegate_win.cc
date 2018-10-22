// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/system_menu_insertion_delegate_win.h"

#include <algorithm>

int SystemMenuInsertionDelegateWin::GetInsertionIndex(HMENU native_menu) {
  return std::max(0, GetMenuItemCount(native_menu) - 1);
}
