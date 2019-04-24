// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_DIALOG_UTILS_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_DIALOG_UTILS_H_

#include "base/callback_forward.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"

class Browser;

namespace web_app {

enum class InstallResultCode;

// Returns true if a WebApp installation is allowed for the current page.
bool CanCreateWebApp(const Browser* browser);

// Initiates install of a WebApp for the current page.
void CreateWebAppFromCurrentWebContents(Browser* browser,
                                        bool force_shortcut_app);

using WebAppInstalledCallbackForTesting =
    base::OnceCallback<void(const AppId& app_id, InstallResultCode code)>;
void SetInstalledCallbackForTesting(WebAppInstalledCallbackForTesting callback);

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_DIALOG_UTILS_H_
