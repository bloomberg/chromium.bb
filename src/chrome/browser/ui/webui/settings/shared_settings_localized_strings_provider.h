// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SHARED_SETTINGS_LOCALIZED_STRINGS_PROVIDER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SHARED_SETTINGS_LOCALIZED_STRINGS_PROVIDER_H_

namespace content {
class WebUIDataSource;
}  // namespace content

namespace settings {

// Adds strings used by the <settings-captions> element.
void AddCaptionSubpageStrings(content::WebUIDataSource* html_source);

// Adds strings used by the <settings-personalization-options> element.
void AddPersonalizationOptionsStrings(content::WebUIDataSource* html_source);

// Adds strings used by the <settings-sync-controls> element.
void AddSyncControlsStrings(content::WebUIDataSource* html_source);

// Adds strings used by the <settings-sync-account-control> element.
void AddSyncAccountControlStrings(content::WebUIDataSource* html_source);

#if defined(OS_CHROMEOS)
// Adds strings used by the <settings-password-prompt-dialog> element.
void AddPasswordPromptDialogStrings(content::WebUIDataSource* html_source);
#endif

// Adds strings used by the <settings-sync-page> element.
void AddSyncPageStrings(content::WebUIDataSource* html_source);

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SHARED_SETTINGS_LOCALIZED_STRINGS_PROVIDER_H_
