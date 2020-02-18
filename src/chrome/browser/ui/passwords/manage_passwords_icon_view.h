// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_ICON_VIEW_H_
#define CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_ICON_VIEW_H_

#include "base/macros.h"
#include "components/password_manager/core/common/password_manager_ui.h"

// An interface for updating the passwords icon in the location bar.
class ManagePasswordsIconView {
 public:
  ManagePasswordsIconView() {}

  virtual void SetState(password_manager::ui::State state) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ManagePasswordsIconView);
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_ICON_VIEW_H_
