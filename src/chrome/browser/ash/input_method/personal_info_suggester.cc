// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/personal_info_suggester.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/services/ime/public/cpp/suggestions.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/input_method/ui/suggestion_details.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/extensions/input_method_api.h"
#include "chrome/browser/extensions/api/input_ime/input_ime_api.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/ui/label_formatter_utils.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace ash {
namespace input_method {

namespace {

using ::chromeos::ime::TextSuggestion;
using ::chromeos::ime::TextSuggestionMode;
using ::chromeos::ime::TextSuggestionType;

const size_t kMaxConfirmedTextLength = 10;
constexpr size_t kMaxTextBeforeCursorLength = 50;

const char kSingleSubjectRegex[] = "my ";
const char kSingleOrPluralSubjectRegex[] = "(my|our) ";
const char kTriggersRegex[] = "( is:?|:) $";
const char kEmailRegex[] = "email";
const char kNameRegex[] = "(full )?name";
const char kAddressRegex[] =
    "((mailing|postal|shipping|home|delivery|physical|current|billing|correct) "
    ")?address";
const char kPhoneNumberRegex[] =
    "(((phone|mobile|telephone) )?number|phone|telephone)";
const char kFirstNameRegex[] = "first name";
const char kLastNameRegex[] = "last name";

const char16_t kShowPersonalInfoSuggestionMessage[] =
    u"Personal info suggested. Press down arrow to access; escape to ignore.";
const char16_t kDismissPersonalInfoSuggestionMessage[] =
    u"Suggestion dismissed.";
const char16_t kAcceptPersonalInfoSuggestionMessage[] = u"Suggestion inserted.";

// The current personal information would only provide one suggestion, so there
// could be only two possible UI: 1. only one suggestion, 2. one suggestion and
// one learn more button, and the suggestion is always before the learn more
// button. So suggestion could be 1 of 1 or 1 of 2 depending on whether the
// learn more button is displayed, but learn more button can only be 2 of 2.
const char kSuggestionMessageTemplate[] =
    "Suggestion %s. Button. Menu item 1 of %d. Press enter to insert; escape "
    "to dismiss.";
const char16_t kLearnMoreMessage[] =
    u"Learn more about suggestions. Link. Menu item 2 of 2. Press enter to "
    u"activate; escape to dismiss.";
const int kNoneHighlighted = -1;

const std::vector<autofill::ServerFieldType>& GetHomeAddressTypes() {
  static base::NoDestructor<std::vector<autofill::ServerFieldType>>
      homeAddressTypes{
          {autofill::ServerFieldType::ADDRESS_HOME_LINE1,
           autofill::ServerFieldType::ADDRESS_HOME_LINE2,
           autofill::ServerFieldType::ADDRESS_HOME_LINE3,
           autofill::ServerFieldType::ADDRESS_HOME_STREET_ADDRESS,
           autofill::ServerFieldType::ADDRESS_HOME_DEPENDENT_LOCALITY,
           autofill::ServerFieldType::ADDRESS_HOME_CITY,
           autofill::ServerFieldType::ADDRESS_HOME_STATE,
           autofill::ServerFieldType::ADDRESS_HOME_ZIP,
           autofill::ServerFieldType::ADDRESS_HOME_SORTING_CODE,
           autofill::ServerFieldType::ADDRESS_HOME_COUNTRY}};
  return *homeAddressTypes;
}

void RecordTimeToAccept(base::TimeDelta delta) {
  base::UmaHistogramTimes("InputMethod.Assistive.TimeToAccept.PersonalInfo",
                          delta);
}

void RecordTimeToDismiss(base::TimeDelta delta) {
  base::UmaHistogramTimes("InputMethod.Assistive.TimeToDismiss.PersonalInfo",
                          delta);
}

void RecordAssistiveInsufficientData(AssistiveType type) {
  base::UmaHistogramEnumeration("InputMethod.Assistive.InsufficientData", type);
}

TextSuggestion MapToTextSuggestion(std::u16string candidate_string) {
  return {.mode = TextSuggestionMode::kPrediction,
          .type = TextSuggestionType::kAssistivePersonalInfo,
          .text = base::UTF16ToUTF8(candidate_string)};
}

}  // namespace

AssistiveType ProposePersonalInfoAssistiveAction(const std::u16string& text) {
  std::string lower_case_utf8_text =
      base::ToLowerASCII(base::UTF16ToUTF8(text));
  if (!(RE2::PartialMatch(lower_case_utf8_text, " $"))) {
    return AssistiveType::kGenericAction;
  }

  if (base::FeatureList::IsEnabled(features::kAssistPersonalInfoAddress)) {
    if (RE2::PartialMatch(
            lower_case_utf8_text,
            base::StringPrintf("%s%s%s$", kSingleOrPluralSubjectRegex,
                               kAddressRegex, kTriggersRegex))) {
      return AssistiveType::kPersonalAddress;
    }
  }

  if (base::FeatureList::IsEnabled(features::kAssistPersonalInfoEmail)) {
    if (RE2::PartialMatch(lower_case_utf8_text,
                          base::StringPrintf("%s%s%s$", kSingleSubjectRegex,
                                             kEmailRegex, kTriggersRegex))) {
      return AssistiveType::kPersonalEmail;
    }
  }

  if (base::FeatureList::IsEnabled(features::kAssistPersonalInfoName)) {
    if (RE2::PartialMatch(lower_case_utf8_text,
                          base::StringPrintf("%s%s%s$", kSingleSubjectRegex,
                                             kNameRegex, kTriggersRegex))) {
      return AssistiveType::kPersonalName;
    }
    if (RE2::PartialMatch(
            lower_case_utf8_text,
            base::StringPrintf("%s%s%s$", kSingleSubjectRegex, kFirstNameRegex,
                               kTriggersRegex))) {
      return AssistiveType::kPersonalFirstName;
    }
    if (RE2::PartialMatch(lower_case_utf8_text,
                          base::StringPrintf("%s%s%s$", kSingleSubjectRegex,
                                             kLastNameRegex, kTriggersRegex))) {
      return AssistiveType::kPersonalLastName;
    }
  }

  if (base::FeatureList::IsEnabled(features::kAssistPersonalInfoPhoneNumber)) {
    if (RE2::PartialMatch(
            lower_case_utf8_text,
            base::StringPrintf("%s%s%s$", kSingleSubjectRegex,
                               kPhoneNumberRegex, kTriggersRegex))) {
      return AssistiveType::kPersonalPhoneNumber;
    }
  }

  return AssistiveType::kGenericAction;
}

PersonalInfoSuggester::PersonalInfoSuggester(
    SuggestionHandlerInterface* suggestion_handler,
    Profile* profile,
    autofill::PersonalDataManager* personal_data_manager)
    : suggestion_handler_(suggestion_handler),
      profile_(profile),
      personal_data_manager_(
          personal_data_manager
              ? personal_data_manager
              : autofill::PersonalDataManagerFactory::GetForProfile(profile)),
      highlighted_index_(kNoneHighlighted) {
  suggestion_button_.id = ui::ime::ButtonId::kSuggestion;
  suggestion_button_.window_type =
      ui::ime::AssistiveWindowType::kPersonalInfoSuggestion;
  suggestion_button_.index = 0;
  settings_button_.id = ui::ime::ButtonId::kSmartInputsSettingLink;
  settings_button_.announce_string = kLearnMoreMessage;
  settings_button_.window_type =
      ui::ime::AssistiveWindowType::kPersonalInfoSuggestion;
}

PersonalInfoSuggester::~PersonalInfoSuggester() = default;

void PersonalInfoSuggester::OnFocus(int context_id) {
  context_id_ = context_id;
}

void PersonalInfoSuggester::OnBlur() {
  context_id_ = -1;
}

void PersonalInfoSuggester::OnExternalSuggestionsUpdated(
    const std::vector<TextSuggestion>& suggestions) {
  // PersonalInfoSuggester doesn't utilize any suggestions produced externally,
  // so ignore this call.
}

SuggestionStatus PersonalInfoSuggester::HandleKeyEvent(
    const ui::KeyEvent& event) {
  if (!suggestion_shown_)
    return SuggestionStatus::kNotHandled;

  if (event.code() == ui::DomCode::ESCAPE) {
    DismissSuggestion();
    return SuggestionStatus::kDismiss;
  }
  if (highlighted_index_ == kNoneHighlighted && buttons_.size() > 0) {
    if (event.code() == ui::DomCode::ARROW_DOWN ||
        event.code() == ui::DomCode::ARROW_UP) {
      highlighted_index_ =
          event.code() == ui::DomCode::ARROW_DOWN ? 0 : buttons_.size() - 1;
      SetButtonHighlighted(buttons_[highlighted_index_], true);
      return SuggestionStatus::kBrowsing;
    }
  } else {
    if (event.code() == ui::DomCode::ENTER) {
      switch (buttons_[highlighted_index_].id) {
        case ui::ime::ButtonId::kSuggestion:
          AcceptSuggestion();
          return SuggestionStatus::kAccept;
        case ui::ime::ButtonId::kSmartInputsSettingLink:
          suggestion_handler_->ClickButton(buttons_[highlighted_index_]);
          return SuggestionStatus::kOpenSettings;
        default:
          break;
      }
    } else if (event.code() == ui::DomCode::ARROW_UP ||
               event.code() == ui::DomCode::ARROW_DOWN) {
      SetButtonHighlighted(buttons_[highlighted_index_], false);
      if (event.code() == ui::DomCode::ARROW_UP) {
        highlighted_index_ =
            (highlighted_index_ + buttons_.size() - 1) % buttons_.size();
      } else {
        highlighted_index_ = (highlighted_index_ + 1) % buttons_.size();
      }
      SetButtonHighlighted(buttons_[highlighted_index_], true);
      return SuggestionStatus::kBrowsing;
    }
  }

  return SuggestionStatus::kNotHandled;
}

bool PersonalInfoSuggester::Suggest(const std::u16string& text,
                                    size_t cursor_pos,
                                    size_t anchor_pos) {
  // |text| could be very long, we get at most |kMaxTextBeforeCursorLength|
  // characters before cursor.
  int start_pos = cursor_pos >= kMaxTextBeforeCursorLength
                      ? cursor_pos - kMaxTextBeforeCursorLength
                      : 0;
  std::u16string text_before_cursor =
      text.substr(start_pos, cursor_pos - start_pos);

  if (suggestion_shown_) {
    size_t text_length = text_before_cursor.length();
    bool matched = false;
    for (size_t offset = 0;
         offset < suggestion_.length() && offset < text_length &&
         offset < kMaxConfirmedTextLength;
         offset++) {
      std::u16string text_before =
          text_before_cursor.substr(0, text_length - offset);
      std::u16string confirmed_text =
          text_before_cursor.substr(text_length - offset);
      if (base::StartsWith(suggestion_, confirmed_text,
                           base::CompareCase::INSENSITIVE_ASCII) &&
          suggestion_ == GetSuggestion(text_before)) {
        matched = true;
        ShowSuggestion(suggestion_, offset);
        break;
      }
    }
    return matched;
  } else {
    suggestion_ = GetSuggestion(text_before_cursor);
    if (suggestion_.empty()) {
      if (proposed_action_type_ != AssistiveType::kGenericAction)
        RecordAssistiveInsufficientData(proposed_action_type_);
    } else {
      ShowSuggestion(suggestion_, 0);
    }
    return suggestion_shown_;
  }
}

std::u16string PersonalInfoSuggester::GetSuggestion(
    const std::u16string& text) {
  proposed_action_type_ = ProposePersonalInfoAssistiveAction(text);

  if (proposed_action_type_ == AssistiveType::kGenericAction)
    return base::EmptyString16();

  if (proposed_action_type_ == AssistiveType::kPersonalEmail) {
    return profile_ ? base::UTF8ToUTF16(profile_->GetProfileUserName())
                    : base::EmptyString16();
  }

  if (!personal_data_manager_)
    return base::EmptyString16();

  auto autofill_profiles = personal_data_manager_->GetProfilesToSuggest();
  if (autofill_profiles.empty())
    return base::EmptyString16();

  // Currently, we are just picking the first candidate, will improve the
  // strategy in the future.
  auto* profile = autofill_profiles[0];
  std::u16string suggestion;
  const std::string app_locale = g_browser_process->GetApplicationLocale();
  switch (proposed_action_type_) {
    case AssistiveType::kPersonalName:
      suggestion = profile->GetRawInfo(autofill::ServerFieldType::NAME_FULL);
      break;
    case AssistiveType::kPersonalAddress:
      suggestion = autofill::GetLabelNationalAddress(GetHomeAddressTypes(),
                                                     *profile, app_locale);
      break;
    case AssistiveType::kPersonalPhoneNumber:
      suggestion = profile->GetRawInfo(
          autofill::ServerFieldType::PHONE_HOME_WHOLE_NUMBER);
      break;
    case AssistiveType::kPersonalFirstName:
      suggestion = profile->GetRawInfo(autofill::ServerFieldType::NAME_FIRST);
      break;
    case AssistiveType::kPersonalLastName:
      suggestion = profile->GetRawInfo(autofill::ServerFieldType::NAME_LAST);
      break;
    default:
      NOTREACHED();
      break;
  }
  return suggestion;
}

void PersonalInfoSuggester::ShowSuggestion(const std::u16string& text,
                                           const size_t confirmed_length) {
  if (ChromeKeyboardControllerClient::Get()->is_keyboard_visible()) {
    const std::vector<std::string> args{base::UTF16ToUTF8(text)};
    suggestion_handler_->OnSuggestionsChanged(args);
    return;
  }

  if (highlighted_index_ != kNoneHighlighted) {
    SetButtonHighlighted(buttons_[highlighted_index_], false);
    highlighted_index_ = kNoneHighlighted;
  }

  std::string error;
  bool show_accept_annotation =
      GetPrefValue(kPersonalInfoSuggesterAcceptanceCount) < kMaxAcceptanceCount;
  ui::ime::SuggestionDetails details;
  details.text = text;
  details.confirmed_length = confirmed_length;
  details.show_accept_annotation = show_accept_annotation;
  details.show_setting_link =
      GetPrefValue(kPersonalInfoSuggesterAcceptanceCount) == 0 &&
      GetPrefValue(kPersonalInfoSuggesterShowSettingCount) <
          kMaxShowSettingCount;
  suggestion_handler_->SetSuggestion(context_id_, details, &error);
  if (!error.empty()) {
    LOG(ERROR) << "Fail to show suggestion. " << error;
  }

  suggestion_button_.announce_string = base::UTF8ToUTF16(base::StringPrintf(
      kSuggestionMessageTemplate, base::UTF16ToUTF8(text).c_str(),
      details.show_setting_link ? 2 : 1));
  buttons_.clear();
  buttons_.push_back(suggestion_button_);
  if (details.show_setting_link)
    buttons_.push_back(settings_button_);

  if (suggestion_shown_) {
    first_shown_ = false;
  } else {
    session_start_ = base::TimeTicks::Now();
    first_shown_ = true;
    IncrementPrefValueTilCapped(kPersonalInfoSuggesterShowSettingCount,
                                kMaxShowSettingCount);
    // TODO(jiwan): Add translation to other languages when we support
    // more than English.
    suggestion_handler_->Announce(kShowPersonalInfoSuggestionMessage);
  }

  suggestion_shown_ = true;
}

int PersonalInfoSuggester::GetPrefValue(const std::string& pref_name) {
  DictionaryPrefUpdate update(profile_->GetPrefs(),
                              prefs::kAssistiveInputFeatureSettings);
  auto value = update->FindIntKey(pref_name);
  if (!value.has_value()) {
    update->SetIntKey(pref_name, 0);
    return 0;
  }
  return *value;
}

void PersonalInfoSuggester::IncrementPrefValueTilCapped(
    const std::string& pref_name,
    int max_value) {
  int value = GetPrefValue(pref_name);
  if (value < max_value) {
    DictionaryPrefUpdate update(profile_->GetPrefs(),
                                prefs::kAssistiveInputFeatureSettings);
    update->SetIntKey(pref_name, value + 1);
  }
}

AssistiveType PersonalInfoSuggester::GetProposeActionType() {
  return proposed_action_type_;
}

bool PersonalInfoSuggester::HasSuggestions() {
  return suggestion_shown_;
}

std::vector<TextSuggestion> PersonalInfoSuggester::GetSuggestions() {
  if (HasSuggestions())
    return {MapToTextSuggestion(suggestion_)};
  return {};
}

bool PersonalInfoSuggester::AcceptSuggestion(size_t index) {
  std::string error;
  suggestion_handler_->AcceptSuggestion(context_id_, &error);

  if (!error.empty()) {
    LOG(ERROR) << "Failed to accept suggestion. " << error;
    return false;
  }

  RecordTimeToAccept(base::TimeTicks::Now() - session_start_);
  IncrementPrefValueTilCapped(kPersonalInfoSuggesterAcceptanceCount,
                              kMaxAcceptanceCount);
  suggestion_shown_ = false;
  suggestion_handler_->Announce(kAcceptPersonalInfoSuggestionMessage);

  return true;
}

void PersonalInfoSuggester::DismissSuggestion() {
  std::string error;
  suggestion_handler_->DismissSuggestion(context_id_, &error);
  if (!error.empty()) {
    LOG(ERROR) << "Failed to dismiss suggestion. " << error;
    return;
  }
  suggestion_shown_ = false;
  RecordTimeToDismiss(base::TimeTicks::Now() - session_start_);
  suggestion_handler_->Announce(kDismissPersonalInfoSuggestionMessage);
}

void PersonalInfoSuggester::SetButtonHighlighted(
    const ui::ime::AssistiveWindowButton& button,
    bool highlighted) {
  std::string error;
  suggestion_handler_->SetButtonHighlighted(context_id_, button, highlighted,
                                            &error);
  if (!error.empty()) {
    LOG(ERROR) << "Failed to set button highlighted. " << error;
  }
}

}  // namespace input_method
}  // namespace ash
