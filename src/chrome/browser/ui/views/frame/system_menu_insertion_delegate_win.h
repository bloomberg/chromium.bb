// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_SYSTEM_MENU_INSERTION_DELEGATE_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_SYSTEM_MENU_INSERTION_DELEGATE_WIN_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/views/controls/menu/menu_insertion_delegate_win.h"

// SystemMenuInsertionDelegateWin is used to determine the index to insert menu
// items into the system item. It is only needed on windows as that is the only
// place we insert items into the system menu.
class SystemMenuInsertionDelegateWin : public views::MenuInsertionDelegateWin {
 public:
  SystemMenuInsertionDelegateWin() {}
  ~SystemMenuInsertionDelegateWin() override {}

  // Overridden from views::MenuInsertionDelegateWin:
  int GetInsertionIndex(HMENU native_menu) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemMenuInsertionDelegateWin);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_SYSTEM_MENU_INSERTION_DELEGATE_WIN_H_
