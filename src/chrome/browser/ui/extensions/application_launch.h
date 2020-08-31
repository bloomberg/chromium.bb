// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_APPLICATION_LAUNCH_H_
#define CHROME_BROWSER_UI_EXTENSIONS_APPLICATION_LAUNCH_H_

#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "url/gurl.h"

class Browser;
class Profile;

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

namespace content {
class WebContents;
}

namespace extensions {
class Extension;
}

enum class WindowOpenDisposition;

// Opens the application, possibly prompting the user to re-enable it.
void OpenApplicationWithReenablePrompt(Profile* profile,
                                       const apps::AppLaunchParams& params);

// Open the application in a way specified by |params|.
content::WebContents* OpenApplication(Profile* profile,
                                      const apps::AppLaunchParams& params);

// Create the application in a way specified by |params| in a new window but
// delaying activating and showing it.
Browser* CreateApplicationWindow(Profile* profile,
                                 const apps::AppLaunchParams& params,
                                 const GURL& url);

// Navigate application window to application url, but do not show it yet.
content::WebContents* NavigateApplicationWindow(
    Browser* browser,
    const apps::AppLaunchParams& params,
    const GURL& url,
    WindowOpenDisposition disposition);

// Open the application in a way specified by |params| in a new window.
content::WebContents* OpenApplicationWindow(Profile* profile,
                                            const apps::AppLaunchParams& params,
                                            const GURL& url);

// Open |url| in an app shortcut window.
// There are two kinds of app shortcuts: Shortcuts to a URL,
// and shortcuts that open an installed application.  This function
// is used to open the former.  To open the latter, use
// application_launch::OpenApplication().
content::WebContents* OpenAppShortcutWindow(Profile* profile,
                                            const GURL& url);

// Whether the extension can be launched by sending a
// chrome.app.runtime.onLaunched event.
bool CanLaunchViaEvent(const extensions::Extension* extension);

// Attempt to open |app_id| in a new window or tab. Open an empty browser
// window if unsuccessful. The user's preferred launch container for the app
// (standalone window or browser tab) is used. |callback| will be called with
// the container type used to open the app, kLaunchContainerNone if an empty
// browser window was opened.
void LaunchAppWithCallback(
    Profile* profile,
    const std::string& app_id,
    const base::CommandLine& command_line,
    const base::FilePath& current_directory,
    base::OnceCallback<void(Browser* browser,
                            apps::mojom::LaunchContainer container)> callback);

#endif  // CHROME_BROWSER_UI_EXTENSIONS_APPLICATION_LAUNCH_H_
