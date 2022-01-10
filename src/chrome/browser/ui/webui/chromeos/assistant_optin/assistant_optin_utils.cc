// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/assistant_optin/assistant_optin_utils.h"

#include <utility>

#include "ash/components/arc/arc_prefs.h"
#include "ash/components/audio/cras_audio_handler.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/services/assistant/public/cpp/assistant_prefs.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "chromeos/services/assistant/public/proto/activity_control_settings_common.pb.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

using AssistantActivityControlConsent =
    sync_pb::UserConsentTypes::AssistantActivityControlConsent;

namespace chromeos {

void RecordAssistantOptInStatus(AssistantOptInFlowStatus status) {
  UMA_HISTOGRAM_ENUMERATION("Assistant.OptInFlowStatus", status, kMaxValue + 1);
}

void RecordAssistantActivityControlOptInStatus(
    sync_pb::UserConsentTypes::AssistantActivityControlConsent::SettingType
        setting_type,
    bool opted_in) {
  AssistantOptInFlowStatus status;
  switch (setting_type) {
    case AssistantActivityControlConsent::ALL:
    case AssistantActivityControlConsent::SETTING_TYPE_UNSPECIFIED:
      status = opted_in ? ACTIVITY_CONTROL_ACCEPTED : ACTIVITY_CONTROL_SKIPPED;
      break;
    case AssistantActivityControlConsent::WEB_AND_APP_ACTIVITY:
      status = opted_in ? ACTIVITY_CONTROL_WAA_ACCEPTED
                        : ACTIVITY_CONTROL_WAA_SKIPPED;
      break;
    case AssistantActivityControlConsent::DEVICE_APPS:
      status =
          opted_in ? ACTIVITY_CONTROL_DA_ACCEPTED : ACTIVITY_CONTROL_DA_SKIPPED;
      break;
  }
  RecordAssistantOptInStatus(status);
}

// Construct SettingsUiSelector for the ConsentFlow UI.
assistant::SettingsUiSelector GetSettingsUiSelector() {
  assistant::SettingsUiSelector selector;
  assistant::ConsentFlowUiSelector* consent_flow_ui =
      selector.mutable_consent_flow_ui_selector();
  consent_flow_ui->set_flow_id(assistant::ActivityControlSettingsUiSelector::
                                   ASSISTANT_SUW_ONBOARDING_ON_CHROME_OS);
  selector.set_email_opt_in(true);
  selector.set_gaia_user_context_ui(true);
  return selector;
}

// Construct SettingsUiUpdate for user opt-in.
assistant::SettingsUiUpdate GetSettingsUiUpdate(
    const std::string& consent_token) {
  assistant::SettingsUiUpdate update;
  assistant::ConsentFlowUiUpdate* consent_flow_update =
      update.mutable_consent_flow_ui_update();
  consent_flow_update->set_flow_id(
      assistant::ActivityControlSettingsUiSelector::
          ASSISTANT_SUW_ONBOARDING_ON_CHROME_OS);
  consent_flow_update->set_consent_token(consent_token);

  return update;
}

// Helper method to create zippy data.
base::Value CreateZippyData(const ActivityControlUi& activity_control_ui,
                            bool is_minor_mode) {
  base::Value zippy_data(base::Value::Type::LIST);
  auto zippy_list = activity_control_ui.setting_zippy();
  auto learn_more_dialog = activity_control_ui.learn_more_dialog();
  for (auto& setting_zippy : zippy_list) {
    base::Value data(base::Value::Type::DICTIONARY);
    data.SetKey("title", base::Value(activity_control_ui.title()));
    data.SetKey("identity", base::Value(activity_control_ui.identity()));
    if (activity_control_ui.intro_text_paragraph_size()) {
      data.SetKey("intro",
                  base::Value(activity_control_ui.intro_text_paragraph(0)));
    }
    data.SetKey("name", base::Value(setting_zippy.title()));
    if (setting_zippy.description_paragraph_size()) {
      data.SetKey("description",
                  base::Value(setting_zippy.description_paragraph(0)));
    }
    if (setting_zippy.additional_info_paragraph_size()) {
      data.SetKey("additionalInfo",
                  base::Value(setting_zippy.additional_info_paragraph(0)));
    }
    data.SetKey("iconUri", base::Value(setting_zippy.icon_uri()));
    data.SetKey("popupLink", base::Value(l10n_util::GetStringUTF16(
                                 IDS_ASSISTANT_ACTIVITY_CONTROL_POPUP_LINK)));
    if (is_minor_mode) {
      data.SetKey("learnMoreDialogTitle",
                  base::Value(learn_more_dialog.title()));
      if (learn_more_dialog.paragraph_size()) {
        data.SetKey("learnMoreDialogContent",
                    base::Value(learn_more_dialog.paragraph(0).value()));
      }
    } else {
      data.SetKey("learnMoreDialogTitle", base::Value(setting_zippy.title()));
      if (setting_zippy.additional_info_paragraph_size()) {
        data.SetKey("learnMoreDialogContent",
                    base::Value(setting_zippy.additional_info_paragraph(0)));
      }
    }
    data.SetKey("learnMoreDialogButton",
                base::Value(learn_more_dialog.dismiss_button()));
    data.SetKey("isMinorMode", base::Value(is_minor_mode));
    zippy_data.Append(std::move(data));
  }
  return zippy_data;
}

// Helper method to create disclosure data.
base::Value CreateDisclosureData(const SettingZippyList& disclosure_list) {
  base::Value disclosure_data(base::Value::Type::LIST);
  for (auto& disclosure : disclosure_list) {
    base::Value data(base::Value::Type::DICTIONARY);
    data.SetKey("title", base::Value(disclosure.title()));
    if (disclosure.description_paragraph_size()) {
      data.SetKey("description",
                  base::Value(disclosure.description_paragraph(0)));
    }
    if (disclosure.additional_info_paragraph_size()) {
      data.SetKey("additionalInfo",
                  base::Value(disclosure.additional_info_paragraph(0)));
    }
    data.SetKey("iconUri", base::Value(disclosure.icon_uri()));
    disclosure_data.Append(std::move(data));
  }
  return disclosure_data;
}

// Get string constants for settings ui.
base::Value GetSettingsUiStrings(const assistant::SettingsUi& settings_ui,
                                 bool activity_control_needed,
                                 bool equal_weight_buttons) {
  auto consent_ui = settings_ui.consent_flow_ui().consent_ui();
  auto activity_control_ui = consent_ui.activity_control_ui();
  base::Value dictionary(base::Value::Type::DICTIONARY);

  dictionary.SetKey("activityControlNeeded",
                    base::Value(activity_control_needed));
  dictionary.SetKey("equalWeightButtons", base::Value(equal_weight_buttons));

  // Add activity control string constants.
  if (activity_control_needed) {
    dictionary.SetKey("valuePropTitle",
                      base::Value(activity_control_ui.title()));
    if (activity_control_ui.footer_paragraph_size()) {
      dictionary.SetKey("valuePropFooter",
                        base::Value(activity_control_ui.footer_paragraph(0)));
    }
    dictionary.SetKey("valuePropNextButton",
                      base::Value(consent_ui.accept_button_text()));
    dictionary.SetKey("valuePropSkipButton",
                      base::Value(consent_ui.reject_button_text()));
  }

  return dictionary;
}

void RecordActivityControlConsent(
    Profile* profile,
    std::string ui_audit_key,
    bool opted_in,
    AssistantActivityControlConsent::SettingType setting_type) {
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  // This function doesn't care about browser sync consent.
  DCHECK(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  const CoreAccountId account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);

  using sync_pb::UserConsentTypes;
  UserConsentTypes::AssistantActivityControlConsent consent;
  consent.set_ui_audit_key(ui_audit_key);
  consent.set_status(opted_in ? UserConsentTypes::GIVEN
                              : UserConsentTypes::NOT_GIVEN);
  consent.set_setting_type(setting_type);

  ConsentAuditorFactory::GetForProfile(profile)
      ->RecordAssistantActivityControlConsent(account_id, consent);
}

bool IsHotwordDspAvailable() {
  return chromeos::CrasAudioHandler::Get()->HasHotwordDevice();
}

bool IsVoiceMatchEnforcedOff(const PrefService* prefs,
                             bool is_oobe_in_progress) {
  // If the hotword preference is managed to always disabled Voice Match flow is
  // hidden.
  if (prefs->IsManagedPreference(assistant::prefs::kAssistantHotwordEnabled) &&
      !prefs->GetBoolean(assistant::prefs::kAssistantHotwordEnabled)) {
    return true;
  }
  // If Voice Match is disabled by policy during OOBE, then Voice Match flow is
  // hidden.
  if (is_oobe_in_progress &&
      !prefs->GetBoolean(
          assistant::prefs::kAssistantVoiceMatchEnabledDuringOobe)) {
    return true;
  }
  return false;
}

AssistantActivityControlConsent::SettingType
GetActivityControlConsentSettingType(const SettingZippyList& setting_zippy) {
  if (setting_zippy.size() > 1) {
    return AssistantActivityControlConsent::ALL;
  }
  auto setting_id = setting_zippy[0].setting_set_id();
  if (setting_id == assistant::SettingSetId::DA) {
    return AssistantActivityControlConsent::DEVICE_APPS;
  }
  if (setting_id == assistant::SettingSetId::WAA) {
    return AssistantActivityControlConsent::WEB_AND_APP_ACTIVITY;
  }
  return AssistantActivityControlConsent::SETTING_TYPE_UNSPECIFIED;
}

}  // namespace chromeos
