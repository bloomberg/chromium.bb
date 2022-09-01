// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/sync_consent_screen_handler.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/ash/login/screens/sync_consent_screen.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "components/user_manager/user_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"

namespace {

// This helper function gets strings from WebUI and a set of known string
// resource ids, and converts strings back to IDs. It CHECKs if string is not
// found in resources.
void GetConsentIDs(const std::unordered_map<std::string, int>& known_strings,
                   const login::StringList& consent_description,
                   const std::string& consent_confirmation,
                   std::vector<int>* consent_description_ids,
                   int* consent_confirmation_id) {
  // The strings returned by the WebUI are not free-form, they must belong into
  // a pre-determined set of strings (stored in `string_to_grd_id_map_`). As
  // this has privacy and legal implications, CHECK the integrity of the strings
  // received from the renderer process before recording the consent.
  for (const std::string& text : consent_description) {
    auto iter = known_strings.find(text);
    CHECK(iter != known_strings.end()) << "Unexpected string:\n" << text;
    consent_description_ids->push_back(iter->second);
  }

  auto iter = known_strings.find(consent_confirmation);
  CHECK(iter != known_strings.end()) << "Unexpected string:\n"
                                     << consent_confirmation;
  *consent_confirmation_id = iter->second;
}

}  // namespace

namespace chromeos {

constexpr StaticOobeScreenId SyncConsentScreenView::kScreenId;

SyncConsentScreenHandler::SyncConsentScreenHandler()
    : BaseScreenHandler(kScreenId) {
  set_user_acted_method_path_deprecated("login.SyncConsentScreen.userActed");
}

SyncConsentScreenHandler::~SyncConsentScreenHandler() = default;

std::string Sanitize(const std::u16string& raw_string) {
  // When the strings are passed to the HTML, the Unicode NBSP symbol
  // (\u00A0) will be automatically replaced with "&nbsp;". This change must
  // be mirrored in the string-to-ids map. Note that "\u00A0" is actually two
  // characters, so we must use base::ReplaceSubstrings* rather than
  // base::ReplaceChars.
  // TODO(alemate): Find a more elegant solution.
  std::string sanitized_string = base::UTF16ToUTF8(raw_string);
  base::ReplaceSubstringsAfterOffset(&sanitized_string, 0, "\u00A0" /* NBSP */,
                                     "&nbsp;");
  return sanitized_string;
}

void SyncConsentScreenHandler::RememberLocalizedValue(
    const std::string& name,
    const int resource_id,
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add(name, resource_id);
  std::u16string raw_string = l10n_util::GetStringUTF16(resource_id);
  known_strings_[Sanitize(raw_string)] = resource_id;
}

void SyncConsentScreenHandler::RememberLocalizedValueWithDeviceName(
    const std::string& name,
    const int resource_id,
    ::login::LocalizedValuesBuilder* builder) {
  builder->AddF(name, resource_id, ui::GetChromeOSDeviceName());

  std::vector<std::u16string> str_substitute;
  str_substitute.push_back(ui::GetChromeOSDeviceName());
  std::u16string raw_string = base::ReplaceStringPlaceholders(
      l10n_util::GetStringUTF16(resource_id), str_substitute, nullptr);
  known_strings_[Sanitize(raw_string)] = resource_id;
}

void SyncConsentScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  // Title and subtitle.
  RememberLocalizedValueWithDeviceName(
      "syncConsentScreenTitle", IDS_LOGIN_SYNC_CONSENT_SCREEN_TITLE_WITH_DEVICE,
      builder);
  RememberLocalizedValue("syncConsentScreenSubtitle",
                         IDS_LOGIN_SYNC_CONSENT_SCREEN_SUBTITLE_2, builder);
  RememberLocalizedValue(
      "syncConsentScreenTitleArcRestrictions",
      IDS_LOGIN_SYNC_CONSENT_SCREEN_TITLE_WITH_ARC_RESTRICTED, builder);

  // Content section.
  RememberLocalizedValueWithDeviceName(
      "syncConsentScreenOsSyncTitle",
      IDS_LOGIN_SYNC_CONSENT_SCREEN_OS_SYNC_NAME_2, builder);
  RememberLocalizedValue(
      "syncConsentScreenChromeBrowserSyncTitle",
      IDS_LOGIN_SYNC_CONSENT_SCREEN_CHROME_BROWSER_SYNC_NAME_2, builder);
  RememberLocalizedValue(
      "syncConsentScreenChromeBrowserSyncDescription",
      IDS_LOGIN_SYNC_CONSENT_SCREEN_CHROME_BROWSER_SYNC_DESCRIPTION, builder);
  RememberLocalizedValueWithDeviceName(
      "syncConsentScreenOsSyncDescriptionArcRestrictions",
      IDS_LOGIN_SYNC_CONSENT_SCREEN_OS_SYNC_DESCRIPTION_WITH_ARC_RESTRICTED,
      builder);

  // Review sync options strings.
  RememberLocalizedValue(
      "syncConsentReviewSyncOptionsText",
      IDS_LOGIN_SYNC_CONSENT_SCREEN_REVIEW_SYNC_OPTIONS_LATER, builder);
  RememberLocalizedValue(
      "syncConsentReviewSyncOptionsWithArcRestrictedText",
      IDS_LOGIN_SYNC_CONSENT_SCREEN_REVIEW_SYNC_OPTIONS_LATER_ARC_RESTRICTED,
      builder);

  // Bottom buttons strings.
  RememberLocalizedValue("syncConsentAcceptAndContinue",
                         IDS_LOGIN_SYNC_CONSENT_SCREEN_ACCEPT_AND_CONTINUE,
                         builder);
  RememberLocalizedValue("syncConsentTurnOnSync",
                         IDS_LOGIN_SYNC_CONSENT_SCREEN_TURN_ON_SYNC, builder);
  RememberLocalizedValue("syncConsentScreenDecline",
                         IDS_LOGIN_SYNC_CONSENT_SCREEN_DECLINE2, builder);
}

void SyncConsentScreenHandler::Bind(SyncConsentScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreenDeprecated(screen);
}

void SyncConsentScreenHandler::Show(bool is_arc_restricted) {
  auto* user_manager = user_manager::UserManager::Get();
  base::Value::Dict data;
  data.Set("isChildAccount", user_manager->IsLoggedInAsChildUser());
  data.Set("isArcRestricted", is_arc_restricted);
  ShowInWebUI(std::move(data));
}

void SyncConsentScreenHandler::Hide() {}

void SyncConsentScreenHandler::ShowLoadedStep() {
  CallJS("login.SyncConsentScreen.showLoadedStep");
}

void SyncConsentScreenHandler::SetIsMinorMode(bool value) {
  CallJS("login.SyncConsentScreen.setIsMinorMode", value);
}

void SyncConsentScreenHandler::InitializeDeprecated() {}

void SyncConsentScreenHandler::RegisterMessages() {
  AddCallback("login.SyncConsentScreen.continue",
              &SyncConsentScreenHandler::HandleContinue);
}

void SyncConsentScreenHandler::HandleContinue(
    const bool opted_in,
    const bool review_sync,
    const base::Value::List& consent_description_list,
    const std::string& consent_confirmation) {
  auto consent_description =
      login::ConvertToStringList(consent_description_list);
  std::vector<int> consent_description_ids;
  int consent_confirmation_id;
  GetConsentIDs(known_strings_, consent_description, consent_confirmation,
                &consent_description_ids, &consent_confirmation_id);
  screen_->OnContinue(opted_in, review_sync, consent_description_ids,
                      consent_confirmation_id);
  SyncConsentScreen::SyncConsentScreenTestDelegate* test_delegate =
      screen_->GetDelegateForTesting();
  if (test_delegate) {
    test_delegate->OnConsentRecordedStrings(consent_description,
                                            consent_confirmation);
  }
}

}  // namespace chromeos
