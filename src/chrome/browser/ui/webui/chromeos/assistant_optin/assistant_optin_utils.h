// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_ASSISTANT_OPTIN_ASSISTANT_OPTIN_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_ASSISTANT_OPTIN_ASSISTANT_OPTIN_UTILS_H_

#include <string>

#include "base/callback.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/services/assistant/public/mojom/constants.mojom.h"
#include "chromeos/services/assistant/public/proto/settings_ui.pb.h"
#include "components/prefs/pref_service.h"
#include "services/service_manager/public/cpp/connector.h"

namespace chromeos {

// Type of Assistant opt-in flow status. This enum is used to back an UMA
// histogram and should be treated as append-only.
enum AssistantOptInFlowStatus {
  FLOW_STARTED = 0,
  ACTIVITY_CONTROL_SHOWN,
  ACTIVITY_CONTROL_ACCEPTED,
  ACTIVITY_CONTROL_SKIPPED,
  THIRD_PARTY_SHOWN,
  THIRD_PARTY_CONTINUED,
  GET_MORE_SHOWN,
  EMAIL_OPTED_IN,
  EMAIL_OPTED_OUT,
  GET_MORE_CONTINUED,
  READY_SCREEN_SHOWN,
  READY_SCREEN_CONTINUED,
  // Magic constant used by the histogram macros.
  kMaxValue = READY_SCREEN_CONTINUED
};

void RecordAssistantOptInStatus(AssistantOptInFlowStatus);

// Construct SettingsUiSelector for the ConsentFlow UI.
assistant::SettingsUiSelector GetSettingsUiSelector();

// Construct SettingsUiUpdate for user opt-in.
assistant::SettingsUiUpdate GetSettingsUiUpdate(
    const std::string& consent_token);

// Construct SettingsUiUpdate for email opt-in.
assistant::SettingsUiUpdate GetEmailOptInUpdate(bool opted_in);

using SettingZippyList = google::protobuf::RepeatedPtrField<
    assistant::ClassicActivityControlUiTexts::SettingZippy>;
// Helper method to create zippy data.
base::Value CreateZippyData(const SettingZippyList& zippy_list);

// Helper method to create disclosure data.
base::Value CreateDisclosureData(const SettingZippyList& disclosure_list);

// Helper method to create get more screen data.
base::Value CreateGetMoreData(bool email_optin_needed,
                              const assistant::EmailOptInUi& email_optin_ui);

// Get string constants for settings ui.
base::Value GetSettingsUiStrings(const assistant::SettingsUi& settings_ui,
                                 bool activity_control_needed);

void RecordActivityControlConsent(Profile* profile,
                                  std::string ui_audit_key,
                                  bool opted_in);

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_ASSISTANT_OPTIN_ASSISTANT_OPTIN_UTILS_H_
