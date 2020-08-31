// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_SEARCH_SECTION_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_SEARCH_SECTION_H_

#include "ash/public/cpp/assistant/assistant_state_base.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_section.h"

namespace content {
class WebUIDataSource;
}  // namespace content

namespace chromeos {
namespace settings {

class SearchTagRegistry;

// Provides UI strings and search tags for Search & Assistant settings. Search
// tags for Assistant settings are added/removed depending on whether the
// feature and relevant flags are enabled/disabled.
class SearchSection : public OsSettingsSection,
                      public ash::AssistantStateObserver {
 public:
  SearchSection(Profile* profile, SearchTagRegistry* search_tag_registry);
  ~SearchSection() override;

 private:
  // OsSettingsSection:
  void AddLoadTimeData(content::WebUIDataSource* html_source) override;
  void AddHandlers(content::WebUI* web_ui) override;

  // ash::AssistantStateObserver:
  void OnAssistantConsentStatusChanged(int consent_status) override;
  void OnAssistantContextEnabled(bool enabled) override;
  void OnAssistantSettingsEnabled(bool enabled) override;
  void OnAssistantHotwordEnabled(bool enabled) override;

  bool IsAssistantAllowed();
  void UpdateAssistantSearchTags();
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_SEARCH_SECTION_H_
