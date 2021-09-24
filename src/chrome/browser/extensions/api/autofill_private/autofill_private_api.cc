// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/autofill_private/autofill_private_api.h"

#include <stddef.h>
#include <memory>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/guid.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/autofill_private/autofill_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/autofill_private.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/autofill_address_util.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/form_data_importer.h"
#include "components/autofill/core/browser/payments/local_card_migration_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_registry.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_ui.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_ui_component.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/localization.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill_private = extensions::api::autofill_private;
namespace addressinput = i18n::addressinput;

namespace {

static const char kSettingsOrigin[] = "Chrome settings";
static const char kErrorDataUnavailable[] = "Autofill data unavailable.";

// Constant to assign a user-verified verification status to the autofill
// profile.
constexpr auto kUserVerified =
    autofill::structured_address::VerificationStatus::kUserVerified;

// Dictionary keys used for serializing AddressUiComponent. Those values
// are used as keys in JavaScript code and shouldn't be modified.
constexpr char kFieldTypeKey[] = "field";
constexpr char kFieldLengthKey[] = "isLongField";
constexpr char kFieldNameKey[] = "fieldName";

// Field names for the address components.
constexpr char kFullNameField[] = "FULL_NAME";
constexpr char kCompanyNameField[] = "COMPANY_NAME";
constexpr char kAddressLineField[] = "ADDRESS_LINES";
constexpr char kDependentLocalityField[] = "ADDRESS_LEVEL_3";
constexpr char kCityField[] = "ADDRESS_LEVEL_2";
constexpr char kStateField[] = "ADDRESS_LEVEL_1";
constexpr char kPostalCodeField[] = "POSTAL_CODE";
constexpr char kSortingCodeField[] = "SORTING_CODE";
constexpr char kCountryField[] = "COUNTY_CODE";

// Converts an autofill::ServerFieldType to string format. Used in serilization
// of field type info to be used in JavaScript code, and hence those values
// shouldn't be modified.
const char* GetStringFromAddressField(i18n::addressinput::AddressField type) {
  switch (type) {
    case i18n::addressinput::RECIPIENT:
      return kFullNameField;
    case i18n::addressinput::ORGANIZATION:
      return kCompanyNameField;
    case i18n::addressinput::STREET_ADDRESS:
      return kAddressLineField;
    case i18n::addressinput::DEPENDENT_LOCALITY:
      return kDependentLocalityField;
    case i18n::addressinput::LOCALITY:
      return kCityField;
    case i18n::addressinput::ADMIN_AREA:
      return kStateField;
    case i18n::addressinput::POSTAL_CODE:
      return kPostalCodeField;
    case i18n::addressinput::SORTING_CODE:
      return kSortingCodeField;
    case i18n::addressinput::COUNTRY:
      return kCountryField;
    default:
      NOTREACHED();
      return "";
  }
}

// Serializes the AddressUiComponent a map from string to base::Value().
base::flat_map<std::string, base::Value> AddressUiComponentAsValueMap(
    const i18n::addressinput::AddressUiComponent& address_ui_component) {
  base::flat_map<std::string, base::Value> info;
  info.emplace(kFieldNameKey, address_ui_component.name);
  info.emplace(kFieldTypeKey,
               GetStringFromAddressField(address_ui_component.field));
  info.emplace(kFieldLengthKey,
               address_ui_component.length_hint ==
                   i18n::addressinput::AddressUiComponent::HINT_LONG);
  return info;
}

// Searches the |list| for the value at |index|.  If this value is present in
// any of the rest of the list, then the item (at |index|) is removed. The
// comparison of phone number values is done on normalized versions of the phone
// number values.
void RemoveDuplicatePhoneNumberAtIndex(size_t index,
                                       const std::string& country_code,
                                       base::Value* list_value) {
  DCHECK(list_value->is_list());
  base::Value::ListView list = list_value->GetList();
  if (list.size() <= index) {
    NOTREACHED() << "List should have a value at index " << index;
    return;
  }
  const std::string& new_value = list[index].GetString();

  bool is_duplicate = false;
  std::string app_locale = g_browser_process->GetApplicationLocale();
  for (size_t i = 0; i < list.size() && !is_duplicate; ++i) {
    if (i == index)
      continue;

    const std::string& existing_value = list[i].GetString();
    is_duplicate = autofill::i18n::PhoneNumbersMatch(
        base::UTF8ToUTF16(new_value), base::UTF8ToUTF16(existing_value),
        country_code, app_locale);
  }

  if (is_duplicate)
    list_value->EraseListIter(list.begin() + index);
}

autofill::BrowserAutofillManager* GetBrowserAutofillManager(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }
  autofill::ContentAutofillDriver* autofill_driver =
      autofill::ContentAutofillDriverFactory::FromWebContents(web_contents)
          ->DriverForFrame(web_contents->GetMainFrame());
  if (!autofill_driver)
    return nullptr;
  return autofill_driver->browser_autofill_manager();
}

}  // namespace

namespace extensions {

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateSaveAddressFunction

ExtensionFunction::ResponseAction AutofillPrivateSaveAddressFunction::Run() {
  std::unique_ptr<api::autofill_private::SaveAddress::Params> parameters =
      api::autofill_private::SaveAddress::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  autofill::PersonalDataManager* personal_data =
      autofill::PersonalDataManagerFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));
  if (!personal_data || !personal_data->IsDataLoaded())
    return RespondNow(Error(kErrorDataUnavailable));

  api::autofill_private::AddressEntry* address = &parameters->address;

  // If a profile guid is specified, get a copy of the profile identified by it.
  // Otherwise create a new one.
  std::string guid = address->guid ? *address->guid : "";
  const bool use_existing_profile = !guid.empty();
  const autofill::AutofillProfile* existing_profile = nullptr;
  if (use_existing_profile) {
    existing_profile = personal_data->GetProfileByGUID(guid);
    if (!existing_profile)
      return RespondNow(Error(kErrorDataUnavailable));
  }
  autofill::AutofillProfile profile =
      existing_profile
          ? *existing_profile
          : autofill::AutofillProfile(base::GenerateGUID(), kSettingsOrigin);

  if (address->full_names) {
    std::string full_name;
    if (!address->full_names->empty())
      full_name = address->full_names->at(0);
    profile.SetInfoWithVerificationStatus(
        autofill::AutofillType(autofill::NAME_FULL),
        base::UTF8ToUTF16(full_name), g_browser_process->GetApplicationLocale(),
        kUserVerified);
  }

  if (address->honorific) {
    profile.SetRawInfoWithVerificationStatus(
        autofill::NAME_HONORIFIC_PREFIX, base::UTF8ToUTF16(*address->honorific),
        kUserVerified);
  }

  if (address->company_name) {
    profile.SetRawInfoWithVerificationStatus(
        autofill::COMPANY_NAME, base::UTF8ToUTF16(*address->company_name),
        kUserVerified);
  }

  if (address->address_lines) {
    profile.SetRawInfoWithVerificationStatus(
        autofill::ADDRESS_HOME_STREET_ADDRESS,
        base::UTF8ToUTF16(*address->address_lines), kUserVerified);
  }

  if (address->address_level1) {
    profile.SetRawInfoWithVerificationStatus(
        autofill::ADDRESS_HOME_STATE,
        base::UTF8ToUTF16(*address->address_level1), kUserVerified);
  }

  if (address->address_level2) {
    profile.SetRawInfoWithVerificationStatus(
        autofill::ADDRESS_HOME_CITY,
        base::UTF8ToUTF16(*address->address_level2), kUserVerified);
  }

  if (address->address_level3) {
    profile.SetRawInfoWithVerificationStatus(
        autofill::ADDRESS_HOME_DEPENDENT_LOCALITY,
        base::UTF8ToUTF16(*address->address_level3), kUserVerified);
  }

  if (address->postal_code) {
    profile.SetRawInfoWithVerificationStatus(
        autofill::ADDRESS_HOME_ZIP, base::UTF8ToUTF16(*address->postal_code),
        kUserVerified);
  }

  if (address->sorting_code) {
    profile.SetRawInfoWithVerificationStatus(
        autofill::ADDRESS_HOME_SORTING_CODE,
        base::UTF8ToUTF16(*address->sorting_code), kUserVerified);
  }

  if (address->country_code) {
    profile.SetRawInfoWithVerificationStatus(
        autofill::ADDRESS_HOME_COUNTRY,
        base::UTF8ToUTF16(*address->country_code), kUserVerified);
  }

  if (address->phone_numbers) {
    std::string phone;
    if (!address->phone_numbers->empty())
      phone = address->phone_numbers->at(0);
    profile.SetRawInfoWithVerificationStatus(autofill::PHONE_HOME_WHOLE_NUMBER,
                                             base::UTF8ToUTF16(phone),
                                             kUserVerified);
  }

  if (address->email_addresses) {
    std::string email;
    if (!address->email_addresses->empty())
      email = address->email_addresses->at(0);
    profile.SetRawInfoWithVerificationStatus(
        autofill::EMAIL_ADDRESS, base::UTF8ToUTF16(email), kUserVerified);
  }

  if (address->language_code)
    profile.set_language_code(*address->language_code);

  if (use_existing_profile) {
    profile.set_origin(kSettingsOrigin);
    personal_data->UpdateProfile(profile);
  } else {
    profile.FinalizeAfterImport();
    personal_data->AddProfile(profile);
  }

  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateGetCountryListFunction

ExtensionFunction::ResponseAction AutofillPrivateGetCountryListFunction::Run() {
  autofill::PersonalDataManager* personal_data =
      autofill::PersonalDataManagerFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));

  // Return an empty list if data is not loaded.
  if (!(personal_data && personal_data->IsDataLoaded())) {
    autofill_util::CountryEntryList empty_list;
    return RespondNow(ArgumentList(
        api::autofill_private::GetCountryList::Results::Create(empty_list)));
  }

  autofill_util::CountryEntryList country_list =
      autofill_util::GenerateCountryList(*personal_data);

  return RespondNow(ArgumentList(
      api::autofill_private::GetCountryList::Results::Create(country_list)));
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateGetAddressComponentsFunction

ExtensionFunction::ResponseAction
AutofillPrivateGetAddressComponentsFunction::Run() {
  std::unique_ptr<api::autofill_private::GetAddressComponents::Params>
      parameters =
          api::autofill_private::GetAddressComponents::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  std::vector<std::vector<::i18n::addressinput::AddressUiComponent>> lines;
  std::string language_code;

  autofill::GetAddressComponents(
      parameters->country_code, g_browser_process->GetApplicationLocale(),
      /*include_literals=*/false, &lines, &language_code);
  // Convert std::vector<std::vector<::i18n::addressinput::AddressUiComponent>>
  // to AddressComponents
  base::Value address_components(base::Value::Type::DICTIONARY);
  base::Value rows(base::Value::Type::LIST);

  for (auto& line : lines) {
    std::vector<base::Value> row_values;
    for (const ::i18n::addressinput::AddressUiComponent& component : line) {
      row_values.emplace_back(AddressUiComponentAsValueMap(component));
    }
    base::Value row(base::Value::Type::DICTIONARY);
    row.SetKey("row", base::Value(std::move(row_values)));
    rows.Append(std::move(row));
  }

  address_components.SetKey("components", std::move(rows));
  address_components.SetKey("languageCode", base::Value(language_code));

  return RespondNow(OneArgument(std::move(address_components)));
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateGetAddressListFunction

ExtensionFunction::ResponseAction AutofillPrivateGetAddressListFunction::Run() {
  autofill::PersonalDataManager* personal_data =
      autofill::PersonalDataManagerFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));

  DCHECK(personal_data && personal_data->IsDataLoaded());

  autofill_util::AddressEntryList address_list =
      autofill_util::GenerateAddressList(*personal_data);
  return RespondNow(ArgumentList(
      api::autofill_private::GetAddressList::Results::Create(address_list)));
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateSaveCreditCardFunction

ExtensionFunction::ResponseAction AutofillPrivateSaveCreditCardFunction::Run() {
  std::unique_ptr<api::autofill_private::SaveCreditCard::Params> parameters =
      api::autofill_private::SaveCreditCard::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  autofill::PersonalDataManager* personal_data =
      autofill::PersonalDataManagerFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));
  if (!personal_data || !personal_data->IsDataLoaded())
    return RespondNow(Error(kErrorDataUnavailable));

  api::autofill_private::CreditCardEntry* card = &parameters->card;

  // If a card guid is specified, get a copy of the card identified by it.
  // Otherwise create a new one.
  std::string guid = card->guid ? *card->guid : "";
  const bool use_existing_card = !guid.empty();
  const autofill::CreditCard* existing_card = nullptr;
  if (use_existing_card) {
    existing_card = personal_data->GetCreditCardByGUID(guid);
    if (!existing_card)
      return RespondNow(Error(kErrorDataUnavailable));
  }
  autofill::CreditCard credit_card =
      existing_card
          ? *existing_card
          : autofill::CreditCard(base::GenerateGUID(), kSettingsOrigin);

  if (card->name) {
    credit_card.SetRawInfo(autofill::CREDIT_CARD_NAME_FULL,
                           base::UTF8ToUTF16(*card->name));
  }

  if (card->card_number) {
    credit_card.SetRawInfo(autofill::CREDIT_CARD_NUMBER,
                           base::UTF8ToUTF16(*card->card_number));
  }

  if (card->expiration_month) {
    credit_card.SetRawInfo(autofill::CREDIT_CARD_EXP_MONTH,
                           base::UTF8ToUTF16(*card->expiration_month));
  }

  if (card->expiration_year) {
    credit_card.SetRawInfo(autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR,
                           base::UTF8ToUTF16(*card->expiration_year));
  }

  if (card->nickname) {
    credit_card.SetNickname(base::UTF8ToUTF16(*card->nickname));
  }

  if (use_existing_card) {
    // Only updates when the card info changes.
    if (existing_card && existing_card->Compare(credit_card) == 0)
      return RespondNow(NoArguments());

    // Record when nickname is updated.
    if (credit_card.HasNonEmptyValidNickname() &&
        existing_card->nickname() != credit_card.nickname()) {
      base::RecordAction(
          base::UserMetricsAction("AutofillCreditCardsEditedWithNickname"));
    }

    personal_data->UpdateCreditCard(credit_card);
    base::RecordAction(base::UserMetricsAction("AutofillCreditCardsEdited"));
  } else {
    personal_data->AddCreditCard(credit_card);
    base::RecordAction(base::UserMetricsAction("AutofillCreditCardsAdded"));
    if (credit_card.HasNonEmptyValidNickname()) {
      base::RecordAction(
          base::UserMetricsAction("AutofillCreditCardsAddedWithNickname"));
    }
  }

  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateRemoveEntryFunction

ExtensionFunction::ResponseAction AutofillPrivateRemoveEntryFunction::Run() {
  std::unique_ptr<api::autofill_private::RemoveEntry::Params> parameters =
      api::autofill_private::RemoveEntry::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  autofill::PersonalDataManager* personal_data =
      autofill::PersonalDataManagerFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));
  if (!personal_data || !personal_data->IsDataLoaded())
    return RespondNow(Error(kErrorDataUnavailable));

  personal_data->RemoveByGUID(parameters->guid);

  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateValidatePhoneNumbersFunction

ExtensionFunction::ResponseAction
AutofillPrivateValidatePhoneNumbersFunction::Run() {
  std::unique_ptr<api::autofill_private::ValidatePhoneNumbers::Params>
      parameters =
          api::autofill_private::ValidatePhoneNumbers::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  api::autofill_private::ValidatePhoneParams* params = &parameters->params;

  // Extract the phone numbers into a ListValue.
  base::Value phone_numbers(base::Value::Type::LIST);
  for (auto phone_number : params->phone_numbers) {
    phone_numbers.Append(phone_number);
  }

  RemoveDuplicatePhoneNumberAtIndex(params->index_of_new_number,
                                    params->country_code, &phone_numbers);

  return RespondNow(OneArgument(std::move(phone_numbers)));
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateMaskCreditCardFunction

ExtensionFunction::ResponseAction AutofillPrivateMaskCreditCardFunction::Run() {
  std::unique_ptr<api::autofill_private::MaskCreditCard::Params> parameters =
      api::autofill_private::MaskCreditCard::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  autofill::PersonalDataManager* personal_data =
      autofill::PersonalDataManagerFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));
  if (!personal_data || !personal_data->IsDataLoaded())
    return RespondNow(Error(kErrorDataUnavailable));

  personal_data->ResetFullServerCard(parameters->guid);

  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateGetCreditCardListFunction

ExtensionFunction::ResponseAction
AutofillPrivateGetCreditCardListFunction::Run() {
  autofill::PersonalDataManager* personal_data =
      autofill::PersonalDataManagerFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));

  DCHECK(personal_data && personal_data->IsDataLoaded());

  autofill_util::CreditCardEntryList credit_card_list =
      autofill_util::GenerateCreditCardList(*personal_data);
  return RespondNow(
      ArgumentList(api::autofill_private::GetCreditCardList::Results::Create(
          credit_card_list)));
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateMigrateCreditCardsFunction

ExtensionFunction::ResponseAction
AutofillPrivateMigrateCreditCardsFunction::Run() {
  autofill::PersonalDataManager* personal_data =
      autofill::PersonalDataManagerFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));
  if (!personal_data || !personal_data->IsDataLoaded())
    return RespondNow(Error(kErrorDataUnavailable));

  // Get the BrowserAutofillManager from the web contents.
  // BrowserAutofillManager has a pointer to its AutofillClient which owns
  // FormDataImporter.
  autofill::BrowserAutofillManager* autofill_manager =
      GetBrowserAutofillManager(GetSenderWebContents());
  if (!autofill_manager || !autofill_manager->client())
    return RespondNow(Error(kErrorDataUnavailable));

  // Get the FormDataImporter from AutofillClient. FormDataImporter owns
  // LocalCardMigrationManager.
  autofill::FormDataImporter* form_data_importer =
      autofill_manager->client()->GetFormDataImporter();
  if (!form_data_importer)
    return RespondNow(Error(kErrorDataUnavailable));

  // Get local card migration manager from form data importer.
  autofill::LocalCardMigrationManager* local_card_migration_manager =
      form_data_importer->local_card_migration_manager();
  if (!local_card_migration_manager)
    return RespondNow(Error(kErrorDataUnavailable));

  // Since we already check the migration requirements on the settings page, we
  // don't check the migration requirements again.
  local_card_migration_manager->GetMigratableCreditCards();
  local_card_migration_manager->AttemptToOfferLocalCardMigration(
      /*is_from_settings_page=*/true);
  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateLogServerCardLinkClickedFunction

ExtensionFunction::ResponseAction
AutofillPrivateLogServerCardLinkClickedFunction::Run() {
  autofill::PersonalDataManager* personal_data =
      autofill::PersonalDataManagerFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));

  if (!personal_data || !personal_data->IsDataLoaded())
    return RespondNow(Error(kErrorDataUnavailable));

  personal_data->LogServerCardLinkClicked();
  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateSetCreditCardFIDOAuthEnabledStateFunction

ExtensionFunction::ResponseAction
AutofillPrivateSetCreditCardFIDOAuthEnabledStateFunction::Run() {
  // Getting CreditCardAccessManager from WebContents.
  autofill::BrowserAutofillManager* autofill_manager =
      GetBrowserAutofillManager(GetSenderWebContents());
  if (!autofill_manager)
    return RespondNow(Error(kErrorDataUnavailable));
  autofill::CreditCardAccessManager* credit_card_access_manager =
      autofill_manager->credit_card_access_manager();
  if (!credit_card_access_manager)
    return RespondNow(Error(kErrorDataUnavailable));

  std::unique_ptr<
      api::autofill_private::SetCreditCardFIDOAuthEnabledState::Params>
      parameters = api::autofill_private::SetCreditCardFIDOAuthEnabledState::
          Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  credit_card_access_manager->OnSettingsPageFIDOAuthToggled(
      parameters->enabled);
  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateGetUpiIdListFunction

ExtensionFunction::ResponseAction AutofillPrivateGetUpiIdListFunction::Run() {
  autofill::PersonalDataManager* personal_data =
      autofill::PersonalDataManagerFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));
  DCHECK(personal_data && personal_data->IsDataLoaded());

  return RespondNow(
      ArgumentList(api::autofill_private::GetUpiIdList::Results::Create(
          personal_data->GetUpiIds())));
}

}  // namespace extensions
