// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_APP_SESSION_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_APP_SESSION_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "chrome/browser/chromeos/app_mode/kiosk_session_plugin_handler_delegate.h"

class Profile;

namespace content {
class WebContents;
}

namespace extensions {
class AppWindow;
}

namespace chromeos {

class KioskSessionPluginHandler;

// AppSession maintains a kiosk session and handles its lifetime.
class AppSession : public KioskSessionPluginHandlerDelegate {
 public:
  AppSession();
  ~AppSession() override;

  // Initializes an app session.
  void Init(Profile* profile, const std::string& app_id);

  // Invoked when GuestViewManager adds a guest web contents.
  void OnGuestAdded(content::WebContents* guest_web_contents);

 private:
  // AppWindowHandler watches for app window and exits the session when the
  // last window of a given app is closed.
  class AppWindowHandler;

  // BrowserWindowHandler monitors Browser object being created during
  // a kiosk session, log info such as URL so that the code path could be
  // fixed and closes the just opened browser window.
  class BrowserWindowHandler;

  void OnAppWindowAdded(extensions::AppWindow* app_window);
  void OnLastAppWindowClosed();

  // KioskSessionPluginHandlerDelegate
  bool ShouldHandlePlugin(const base::FilePath& plugin_path) const override;
  void OnPluginCrashed(const base::FilePath& plugin_path) override;
  void OnPluginHung(const std::set<int>& hung_plugins) override;

  bool is_shutting_down_ = false;

  std::unique_ptr<AppWindowHandler> app_window_handler_;
  std::unique_ptr<BrowserWindowHandler> browser_window_handler_;
  std::unique_ptr<KioskSessionPluginHandler> plugin_handler_;

  DISALLOW_COPY_AND_ASSIGN(AppSession);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_APP_SESSION_H_
