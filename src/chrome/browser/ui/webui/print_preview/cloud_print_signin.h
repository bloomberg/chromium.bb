// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_CLOUD_PRINT_SIGNIN_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_CLOUD_PRINT_SIGNIN_H_

#include "base/callback_forward.h"

class Browser;

namespace printing {

// Creates a tab with Google 'sign in' or 'add account' page, based on
// passed |add_account| value.
void CreateCloudPrintSigninTab(Browser* browser,
                               bool add_account,
                               base::OnceClosure callback);

}  // namespace printing

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_CLOUD_PRINT_SIGNIN_H_
