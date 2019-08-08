// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_SYSTEM_WEB_APP_UI_UTILS_CHROMEOS_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_SYSTEM_WEB_APP_UI_UTILS_CHROMEOS_H_

#include <utility>

#include "base/optional.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "url/gurl.h"

class Browser;
class Profile;

namespace web_app {

// Returns the PWA system App ID for the given system app type.
base::Optional<web_app::AppId> GetAppIdForSystemWebApp(Profile* profile,
                                                       SystemAppType app_type);

// Launches a System App to the given URL, reusing any existing window for the
// app. Returns the browser for the System App, or nullptr if launch/focus
// failed. |did_create| will reflect whether a new window was created if passed.
Browser* LaunchSystemWebApp(Profile* profile,
                            SystemAppType app_type,
                            const GURL& url = GURL(),
                            bool* did_create = nullptr);

// Returns a browser that is hosting the given system app type, or nullptr if
// not found.
Browser* FindSystemWebAppBrowser(Profile* profile, SystemAppType app_type);

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_SYSTEM_WEB_APP_UI_UTILS_CHROMEOS_H_
