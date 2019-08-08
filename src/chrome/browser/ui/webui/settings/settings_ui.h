// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_UI_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_controller.h"

class Profile;

namespace content {
class WebUIDataSource;
class WebUIMessageHandler;
}  // namespace content

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace settings {

// The WebUI handler for chrome://settings.
class SettingsUI : public content::WebUIController,
                   public content::WebContentsObserver {
 public:
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  explicit SettingsUI(content::WebUI* web_ui);
  ~SettingsUI() override;

#if defined(OS_CHROMEOS)
  // Initializes the WebUI message handlers for OS-specific settings.
  static void InitOSWebUIHandlers(Profile* profile,
                                  content::WebUI* web_ui,
                                  content::WebUIDataSource* html_source);
#endif  // defined(OS_CHROMEOS)

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DocumentLoadedInFrame(
      content::RenderFrameHost* render_frame_host) override;
  void DocumentOnLoadCompletedInMainFrame() override;

 private:
  void AddSettingsPageUIHandler(
      std::unique_ptr<content::WebUIMessageHandler> handler);

  base::Time load_start_time_;

  DISALLOW_COPY_AND_ASSIGN(SettingsUI);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_UI_H_
