// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_test_utils.h"

#include <string>

#include "base/guid.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/randomized_encoder.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/form_field_data_predictions.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/prefs/testing_pref_store.h"
#include "components/security_interstitials/core/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

using base::ASCIIToUTF16;

namespace autofill {

bool operator==(const FormFieldDataPredictions& a,
                const FormFieldDataPredictions& b) {
  auto members = [](const FormFieldDataPredictions& p) {
    return std::tie(p.host_form_signature, p.signature, p.heuristic_type,
                    p.server_type, p.overall_type, p.parseable_name, p.section);
  };
  return members(a) == members(b);
}

bool operator==(const FormDataPredictions& a, const FormDataPredictions& b) {
  return test::WithoutUnserializedData(a.data).SameFormAs(
             test::WithoutUnserializedData(b.data)) &&
         a.signature == b.signature && a.fields == b.fields;
}

namespace test {

namespace {

const int kValidityStateBitfield = 1984;

std::string GetRandomCardNumber() {
  const size_t length = 16;
  std::string value;
  value.reserve(length);
  for (size_t i = 0; i < length; ++i)
    value.push_back(static_cast<char>(base::RandInt('0', '9')));
  return value;
}

}  // namespace

LocalFrameToken GetLocalFrameToken() {
  return LocalFrameToken(base::UnguessableToken::Deserialize(98765, 43210));
}

FormRendererId MakeFormRendererId() {
  static uint32_t counter = 10;
  return FormRendererId(counter++);
}

FieldRendererId MakeFieldRendererId() {
  static uint32_t counter = 10;
  return FieldRendererId(counter++);
}

// Creates new, pairwise distinct FormGlobalIds.
FormGlobalId MakeFormGlobalId() {
  return {GetLocalFrameToken(), MakeFormRendererId()};
}

// Creates new, pairwise distinct FieldGlobalIds.
FieldGlobalId MakeFieldGlobalId() {
  return {GetLocalFrameToken(), MakeFieldRendererId()};
}

void SetFormGroupValues(FormGroup& form_group,
                        const std::vector<FormGroupValue>& values) {
  for (const auto& value : values) {
    form_group.SetRawInfoWithVerificationStatus(
        value.type, base::UTF8ToUTF16(value.value), value.verification_status);
  }
}

void VerifyFormGroupValues(const FormGroup& form_group,
                           const std::vector<FormGroupValue>& values,
                           bool ignore_status) {
  for (const auto& value : values) {
    SCOPED_TRACE(testing::Message()
                 << "Expected for type "
                 << AutofillType::ServerFieldTypeToString(value.type) << "\n\t"
                 << value.value << " with status "
                 << (ignore_status ? "(ignored)" : "")
                 << value.verification_status << "\nFound:"
                 << "\n\t" << form_group.GetRawInfo(value.type)
                 << " with status "
                 << form_group.GetVerificationStatus(value.type));

    EXPECT_EQ(form_group.GetRawInfo(value.type),
              base::UTF8ToUTF16(value.value));
    if (!ignore_status) {
      EXPECT_EQ(form_group.GetVerificationStatus(value.type),
                value.verification_status);
    }
  }
}

std::unique_ptr<PrefService> PrefServiceForTesting() {
  scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
      new user_prefs::PrefRegistrySyncable());
  registry->RegisterBooleanPref(
      RandomizedEncoder::kUrlKeyedAnonymizedDataCollectionEnabled, false);
  registry->RegisterBooleanPref(::prefs::kMixedFormsWarningsEnabled, true);
  return PrefServiceForTesting(registry.get());
}

std::unique_ptr<PrefService> PrefServiceForTesting(
    user_prefs::PrefRegistrySyncable* registry) {
  prefs::RegisterProfilePrefs(registry);

  PrefServiceFactory factory;
  factory.set_user_prefs(base::MakeRefCounted<TestingPrefStore>());
  return factory.Create(registry);
}

void CreateTestFormField(const char* label,
                         const char* name,
                         const char* value,
                         const char* type,
                         FormFieldData* field) {
  field->host_frame = GetLocalFrameToken();
  field->unique_renderer_id = MakeFieldRendererId();
  field->label = ASCIIToUTF16(label);
  field->name = ASCIIToUTF16(name);
  field->value = ASCIIToUTF16(value);
  field->form_control_type = type;
  field->is_focusable = true;
}

void CreateTestSelectField(const char* label,
                           const char* name,
                           const char* value,
                           const std::vector<const char*>& values,
                           const std::vector<const char*>& contents,
                           size_t select_size,
                           FormFieldData* field) {
  // Fill the base attributes.
  CreateTestFormField(label, name, value, "select-one", field);

  field->options.clear();
  CHECK_EQ(values.size(), contents.size());
  for (size_t i = 0; i < std::min(values.size(), contents.size()); ++i) {
    field->options.push_back({
        .value = base::UTF8ToUTF16(values[i]),
        .content = base::UTF8ToUTF16(contents[i]),
    });
  }
}

void CreateTestSelectField(const std::vector<const char*>& values,
                           FormFieldData* field) {
  CreateTestSelectField("", "", "", values, values, values.size(), field);
}

void CreateTestDatalistField(const char* label,
                             const char* name,
                             const char* value,
                             const std::vector<const char*>& values,
                             const std::vector<const char*>& labels,
                             FormFieldData* field) {
  // Fill the base attributes.
  CreateTestFormField(label, name, value, "text", field);

  std::vector<std::u16string> values16(values.size());
  for (size_t i = 0; i < values.size(); ++i)
    values16[i] = base::UTF8ToUTF16(values[i]);

  std::vector<std::u16string> label16(labels.size());
  for (size_t i = 0; i < labels.size(); ++i)
    label16[i] = base::UTF8ToUTF16(labels[i]);

  field->datalist_values = values16;
  field->datalist_labels = label16;
}

void CreateTestAddressFormData(FormData* form, const char* unique_id) {
  std::vector<ServerFieldTypeSet> types;
  CreateTestAddressFormData(form, &types, unique_id);
}

void CreateTestAddressFormData(FormData* form,
                               std::vector<ServerFieldTypeSet>* types,
                               const char* unique_id) {
  form->host_frame = GetLocalFrameToken();
  form->unique_renderer_id = MakeFormRendererId();
  form->name = u"MyForm" + ASCIIToUTF16(unique_id ? unique_id : "");
  form->button_titles = {std::make_pair(
      u"Submit", mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)};
  form->url = GURL("https://myform.com/form.html");
  form->action = GURL("https://myform.com/submit.html");
  form->is_action_empty = true;
  form->main_frame_origin =
      url::Origin::Create(GURL("https://myform_root.com/form.html"));
  types->clear();
  form->submission_event =
      mojom::SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION;

  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form->fields.push_back(field);
  types->push_back({NAME_FIRST});
  test::CreateTestFormField("Middle Name", "middlename", "", "text", &field);
  form->fields.push_back(field);
  types->push_back({NAME_MIDDLE});
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form->fields.push_back(field);
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForMoreStructureInNames)) {
    types->push_back({NAME_LAST, NAME_LAST_SECOND});
  } else {
    types->push_back({NAME_LAST});
  }
  test::CreateTestFormField("Address Line 1", "addr1", "", "text", &field);
  form->fields.push_back(field);
  types->push_back({ADDRESS_HOME_LINE1});
  test::CreateTestFormField("Address Line 2", "addr2", "", "text", &field);
  form->fields.push_back(field);
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForMoreStructureInAddresses)) {
    types->push_back({ADDRESS_HOME_SUBPREMISE, ADDRESS_HOME_LINE2});
  } else {
    types->push_back({ADDRESS_HOME_LINE2});
  }
  test::CreateTestFormField("City", "city", "", "text", &field);
  form->fields.push_back(field);
  types->push_back({ADDRESS_HOME_CITY});
  test::CreateTestFormField("State", "state", "", "text", &field);
  form->fields.push_back(field);
  types->push_back({ADDRESS_HOME_STATE});
  test::CreateTestFormField("Postal Code", "zipcode", "", "text", &field);
  form->fields.push_back(field);
  types->push_back({ADDRESS_HOME_ZIP});
  test::CreateTestFormField("Country", "country", "", "text", &field);
  form->fields.push_back(field);
  types->push_back({ADDRESS_HOME_COUNTRY});
  test::CreateTestFormField("Phone Number", "phonenumber", "", "tel", &field);
  form->fields.push_back(field);
  types->push_back({PHONE_HOME_WHOLE_NUMBER});
  test::CreateTestFormField("Email", "email", "", "email", &field);
  form->fields.push_back(field);
  types->push_back({EMAIL_ADDRESS});
}

void CreateTestPersonalInformationFormData(FormData* form,
                                           const char* unique_id) {
  form->unique_renderer_id = MakeFormRendererId();
  form->name = u"MyForm" + ASCIIToUTF16(unique_id ? unique_id : "");
  form->url = GURL("https://myform.com/form.html");
  form->action = GURL("https://myform.com/submit.html");
  form->main_frame_origin =
      url::Origin::Create(GURL("https://myform_root.com/form.html"));

  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form->fields.push_back(field);
  test::CreateTestFormField("Middle Name", "middlename", "", "text", &field);
  form->fields.push_back(field);
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form->fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "email", &field);
  form->fields.push_back(field);
}

void CreateTestCreditCardFormData(FormData* form,
                                  bool is_https,
                                  bool use_month_type,
                                  bool split_names,
                                  const char* unique_id) {
  form->unique_renderer_id = MakeFormRendererId();
  form->name = u"MyForm" + ASCIIToUTF16(unique_id ? unique_id : "");
  if (is_https) {
    form->url = GURL("https://myform.com/form.html");
    form->action = GURL("https://myform.com/submit.html");
    form->main_frame_origin =
        url::Origin::Create(GURL("https://myform_root.com/form.html"));
  } else {
    form->url = GURL("http://myform.com/form.html");
    form->action = GURL("http://myform.com/submit.html");
    form->main_frame_origin =
        url::Origin::Create(GURL("http://myform_root.com/form.html"));
  }

  FormFieldData field;
  if (split_names) {
    test::CreateTestFormField("First Name on Card", "firstnameoncard", "",
                              "text", &field);
    field.autocomplete_attribute = "cc-given-name";
    form->fields.push_back(field);
    test::CreateTestFormField("Last Name on Card", "lastnameoncard", "", "text",
                              &field);
    field.autocomplete_attribute = "cc-family-name";
    form->fields.push_back(field);
    field.autocomplete_attribute = "";
  } else {
    test::CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);
    form->fields.push_back(field);
  }
  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  form->fields.push_back(field);
  if (use_month_type) {
    test::CreateTestFormField("Expiration Date", "ccmonth", "", "month",
                              &field);
    form->fields.push_back(field);
  } else {
    test::CreateTestFormField("Expiration Date", "ccmonth", "", "text", &field);
    form->fields.push_back(field);
    test::CreateTestFormField("", "ccyear", "", "text", &field);
    form->fields.push_back(field);
  }
  test::CreateTestFormField("CVC", "cvc", "", "text", &field);
  form->fields.push_back(field);
}

FormData WithoutUnserializedData(FormData form) {
  form.url = {};
  form.main_frame_origin = {};
  form.host_frame = {};
  for (FormFieldData& field : form.fields)
    field = WithoutUnserializedData(std::move(field));
  return form;
}

FormFieldData WithoutUnserializedData(FormFieldData field) {
  field.host_frame = {};
  return field;
}

inline void check_and_set(
    FormGroup* profile,
    ServerFieldType type,
    const char* value,
    structured_address::VerificationStatus status =
        structured_address::VerificationStatus::kObserved) {
  if (value) {
    profile->SetRawInfoWithVerificationStatus(type, base::UTF8ToUTF16(value),
                                              status);
  }
}

AutofillProfile GetFullValidProfileForCanada() {
  AutofillProfile profile(base::GenerateGUID(), kEmptyOrigin);
  SetProfileInfo(&profile, "Alice", "", "Wonderland", "alice@wonderland.ca",
                 "Fiction", "666 Notre-Dame Ouest", "Apt 8", "Montreal", "QC",
                 "H3B 2T9", "CA", "15141112233");
  return profile;
}

AutofillProfile GetFullValidProfileForChina() {
  AutofillProfile profile(base::GenerateGUID(), kEmptyOrigin);
  SetProfileInfo(&profile, "John", "H.", "Doe", "johndoe@google.cn", "Google",
                 "100 Century Avenue", "", "赫章县", "毕节地区", "贵州省",
                 "200120", "CN", "+86-21-6133-7666");
  return profile;
}

AutofillProfile GetFullProfile() {
  AutofillProfile profile(base::GenerateGUID(), kEmptyOrigin);
  SetProfileInfo(&profile, "John", "H.", "Doe", "johndoe@hades.com",
                 "Underworld", "666 Erebus St.", "Apt 8", "Elysium", "CA",
                 "91111", "US", "16502111111");
  return profile;
}

AutofillProfile GetFullProfile2() {
  AutofillProfile profile(base::GenerateGUID(), kEmptyOrigin);
  SetProfileInfo(&profile, "Jane", "A.", "Smith", "jsmith@example.com", "ACME",
                 "123 Main Street", "Unit 1", "Greensdale", "MI", "48838", "US",
                 "13105557889");
  return profile;
}

AutofillProfile GetFullCanadianProfile() {
  AutofillProfile profile(base::GenerateGUID(), kEmptyOrigin);
  SetProfileInfo(&profile, "Wayne", "", "Gretzky", "wayne@hockey.com", "NHL",
                 "123 Hockey rd.", "Apt 8", "Moncton", "New Brunswick",
                 "E1A 0A6", "CA", "15068531212");
  return profile;
}

AutofillProfile GetIncompleteProfile1() {
  AutofillProfile profile(base::GenerateGUID(), kEmptyOrigin);
  SetProfileInfo(&profile, "John", "H.", "Doe", "jsmith@example.com", "ACME",
                 "123 Main Street", "Unit 1", "Greensdale", "MI", "48838", "US",
                 "");
  return profile;
}

AutofillProfile GetIncompleteProfile2() {
  AutofillProfile profile(base::GenerateGUID(), kEmptyOrigin);
  SetProfileInfo(&profile, "", "", "", "jsmith@example.com", "", "", "", "", "",
                 "", "", "");
  return profile;
}

AutofillProfile GetVerifiedProfile() {
  AutofillProfile profile(GetFullProfile());
  profile.set_origin(kSettingsOrigin);
  return profile;
}

AutofillProfile GetServerProfile() {
  AutofillProfile profile(AutofillProfile::SERVER_PROFILE, "id1");
  // Note: server profiles don't have email addresses and only have full names.
  SetProfileInfo(&profile, "", "", "", "", "Google, Inc.", "123 Fake St.",
                 "Apt. 42", "Mountain View", "California", "94043", "US",
                 "1.800.555.1234");

  profile.SetInfo(NAME_FULL, u"John K. Doe", "en");
  profile.SetRawInfo(ADDRESS_HOME_SORTING_CODE, u"CEDEX");
  profile.SetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY, u"Santa Clara");

  profile.set_language_code("en");
  profile.SetClientValidityFromBitfieldValue(kValidityStateBitfield);
  profile.set_is_client_validity_states_updated(true);
  profile.set_use_count(7);
  profile.set_use_date(base::Time::FromTimeT(54321));

  profile.GenerateServerProfileIdentifier();

  return profile;
}

AutofillProfile GetServerProfile2() {
  AutofillProfile profile(AutofillProfile::SERVER_PROFILE, "id2");
  // Note: server profiles don't have email addresses.
  SetProfileInfo(&profile, "", "", "", "", "Main, Inc.", "4323 Wrong St.",
                 "Apt. 1032", "Sunnyvale", "California", "10011", "US",
                 "+1 514-123-1234");

  profile.SetInfo(NAME_FULL, u"Jim S. Bristow", "en");
  profile.SetRawInfo(ADDRESS_HOME_SORTING_CODE, u"XEDEC");
  profile.SetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY, u"Santa Monica");

  profile.set_language_code("en");
  profile.SetClientValidityFromBitfieldValue(kValidityStateBitfield);
  profile.set_is_client_validity_states_updated(true);
  profile.set_use_count(14);
  profile.set_use_date(base::Time::FromTimeT(98765));

  profile.GenerateServerProfileIdentifier();

  return profile;
}

CreditCard GetCreditCard() {
  CreditCard credit_card(base::GenerateGUID(), kEmptyOrigin);
  SetCreditCardInfo(&credit_card, "Test User", "4111111111111111" /* Visa */,
                    NextMonth().c_str(), NextYear().c_str(), "1");
  return credit_card;
}

CreditCard GetCreditCard2() {
  CreditCard credit_card(base::GenerateGUID(), kEmptyOrigin);
  SetCreditCardInfo(&credit_card, "Someone Else", "378282246310005" /* AmEx */,
                    NextMonth().c_str(), TenYearsFromNow().c_str(), "1");
  return credit_card;
}

CreditCard GetExpiredCreditCard() {
  CreditCard credit_card(base::GenerateGUID(), kEmptyOrigin);
  SetCreditCardInfo(&credit_card, "Test User", "4111111111111111" /* Visa */,
                    NextMonth().c_str(), LastYear().c_str(), "1");
  return credit_card;
}

CreditCard GetIncompleteCreditCard() {
  CreditCard credit_card(base::GenerateGUID(), kEmptyOrigin);
  SetCreditCardInfo(&credit_card, "", "4111111111111111" /* Visa */,
                    NextMonth().c_str(), NextYear().c_str(), "1");
  return credit_card;
}

CreditCard GetVerifiedCreditCard() {
  CreditCard credit_card(GetCreditCard());
  credit_card.set_origin(kSettingsOrigin);
  return credit_card;
}

CreditCard GetVerifiedCreditCard2() {
  CreditCard credit_card(GetCreditCard2());
  credit_card.set_origin(kSettingsOrigin);
  return credit_card;
}

CreditCard GetMaskedServerCard() {
  CreditCard credit_card(CreditCard::MASKED_SERVER_CARD, "a123");
  test::SetCreditCardInfo(&credit_card, "Bonnie Parker",
                          "2109" /* Mastercard */, NextMonth().c_str(),
                          NextYear().c_str(), "1");
  credit_card.SetNetworkForMaskedCard(kMasterCard);
  credit_card.set_instrument_id(1);
  return credit_card;
}

CreditCard GetMaskedServerCardAmex() {
  CreditCard credit_card(CreditCard::MASKED_SERVER_CARD, "b456");
  test::SetCreditCardInfo(&credit_card, "Justin Thyme", "8431" /* Amex */,
                          NextMonth().c_str(), NextYear().c_str(), "1");
  credit_card.SetNetworkForMaskedCard(kAmericanExpressCard);
  return credit_card;
}

CreditCard GetMaskedServerCardWithNickname() {
  CreditCard credit_card(CreditCard::MASKED_SERVER_CARD, "c789");
  test::SetCreditCardInfo(&credit_card, "Test user", "1111" /* Visa */,
                          NextMonth().c_str(), NextYear().c_str(), "1");
  credit_card.SetNetworkForMaskedCard(kVisaCard);
  credit_card.SetNickname(u"Test nickname");
  return credit_card;
}

CreditCard GetMaskedServerCardWithInvalidNickname() {
  CreditCard credit_card(CreditCard::MASKED_SERVER_CARD, "c789");
  test::SetCreditCardInfo(&credit_card, "Test user", "1111" /* Visa */,
                          NextMonth().c_str(), NextYear().c_str(), "1");
  credit_card.SetNetworkForMaskedCard(kVisaCard);
  credit_card.SetNickname(u"Invalid nickname which is too long");
  return credit_card;
}

CreditCard GetFullServerCard() {
  CreditCard credit_card(CreditCard::FULL_SERVER_CARD, "c123");
  test::SetCreditCardInfo(&credit_card, "Full Carter",
                          "4111111111111111" /* Visa */, NextMonth().c_str(),
                          NextYear().c_str(), "1");
  return credit_card;
}

CreditCard GetRandomCreditCard(CreditCard::RecordType record_type) {
  static const char* const kNetworks[] = {
      kAmericanExpressCard,
      kDinersCard,
      kDiscoverCard,
      kEloCard,
      kGenericCard,
      kJCBCard,
      kMasterCard,
      kMirCard,
      kUnionPay,
      kVisaCard,
  };
  constexpr size_t kNumNetworks = sizeof(kNetworks) / sizeof(kNetworks[0]);
  base::Time::Exploded now;
  AutofillClock::Now().LocalExplode(&now);

  CreditCard credit_card =
      (record_type == CreditCard::LOCAL_CARD)
          ? CreditCard(base::GenerateGUID(), kEmptyOrigin)
          : CreditCard(record_type, base::GenerateGUID().substr(24));
  test::SetCreditCardInfo(
      &credit_card, "Justin Thyme", GetRandomCardNumber().c_str(),
      base::StringPrintf("%d", base::RandInt(1, 12)).c_str(),
      base::StringPrintf("%d", now.year + base::RandInt(1, 4)).c_str(), "1");
  if (record_type == CreditCard::MASKED_SERVER_CARD) {
    credit_card.SetNetworkForMaskedCard(
        kNetworks[base::RandInt(0, kNumNetworks - 1)]);
  }

  return credit_card;
}

CreditCardCloudTokenData GetCreditCardCloudTokenData1() {
  CreditCardCloudTokenData data;
  data.masked_card_id = "data1_id";
  data.suffix = u"1111";
  data.exp_month = 1;
  base::StringToInt(NextYear(), &data.exp_year);
  data.card_art_url = "fake url 1";
  data.instrument_token = "fake token 1";
  return data;
}

CreditCardCloudTokenData GetCreditCardCloudTokenData2() {
  CreditCardCloudTokenData data;
  data.masked_card_id = "data2_id";
  data.suffix = u"2222";
  data.exp_month = 2;
  base::StringToInt(NextYear(), &data.exp_year);
  data.exp_year += 1;
  data.card_art_url = "fake url 2";
  data.instrument_token = "fake token 2";
  return data;
}

AutofillOfferData GetCardLinkedOfferData1() {
  AutofillOfferData data;
  data.offer_id = 111;
  // Sets the expiry to be 45 days later.
  data.expiry = AutofillClock::Now() + base::Days(45);
  data.offer_details_url = GURL("http://www.example1.com");
  data.merchant_origins.emplace_back("http://www.example1.com");
  data.display_strings.value_prop_text = "Get 5% off your purchase";
  data.display_strings.see_details_text = "See details";
  data.display_strings.usage_instructions_text =
      "Check out with this card to activate";
  data.offer_reward_amount = "5%";
  data.eligible_instrument_id.emplace_back(111111);
  return data;
}

AutofillOfferData GetCardLinkedOfferData2() {
  AutofillOfferData data;
  data.offer_id = 222;
  // Sets the expiry to be 40 days later.
  data.expiry = AutofillClock::Now() + base::Days(40);
  data.offer_details_url = GURL("http://www.example2.com");
  data.merchant_origins.emplace_back("http://www.example2.com");
  data.display_strings.value_prop_text = "Get $10 off your purchase";
  data.display_strings.see_details_text = "See details";
  data.display_strings.usage_instructions_text =
      "Check out with this card to activate";
  data.offer_reward_amount = "$10";
  data.eligible_instrument_id.emplace_back(222222);
  return data;
}

AutofillOfferData GetPromoCodeOfferData(GURL origin, bool is_expired) {
  AutofillOfferData data;
  data.offer_id = 333;
  // Sets the expiry to be later if not expired, or earlier if expired.
  data.expiry = is_expired ? AutofillClock::Now() - base::Days(1)
                           : AutofillClock::Now() + base::Days(35);
  data.offer_details_url = GURL("http://www.example.com");
  data.merchant_origins.emplace_back(origin);
  data.display_strings.value_prop_text = "5% off on shoes. Up to $50.";
  data.display_strings.see_details_text = "See details";
  data.display_strings.usage_instructions_text =
      "Click the promo code field at checkout to autofill it.";
  data.promo_code = "5PCTOFFSHOES";
  return data;
}

void SetProfileInfo(AutofillProfile* profile,
                    const char* first_name,
                    const char* middle_name,
                    const char* last_name,
                    const char* email,
                    const char* company,
                    const char* address1,
                    const char* address2,
                    const char* dependent_locality,
                    const char* city,
                    const char* state,
                    const char* zipcode,
                    const char* country,
                    const char* phone,
                    bool finalize,
                    structured_address::VerificationStatus status) {
  check_and_set(profile, NAME_FIRST, first_name, status);
  check_and_set(profile, NAME_MIDDLE, middle_name, status);
  check_and_set(profile, NAME_LAST, last_name, status);
  check_and_set(profile, EMAIL_ADDRESS, email, status);
  check_and_set(profile, COMPANY_NAME, company, status);
  check_and_set(profile, ADDRESS_HOME_LINE1, address1, status);
  check_and_set(profile, ADDRESS_HOME_LINE2, address2, status);
  check_and_set(profile, ADDRESS_HOME_DEPENDENT_LOCALITY, dependent_locality,
                status);
  check_and_set(profile, ADDRESS_HOME_CITY, city, status);
  check_and_set(profile, ADDRESS_HOME_STATE, state, status);
  check_and_set(profile, ADDRESS_HOME_ZIP, zipcode, status);
  check_and_set(profile, ADDRESS_HOME_COUNTRY, country, status);
  check_and_set(profile, PHONE_HOME_WHOLE_NUMBER, phone, status);
  if (finalize)
    profile->FinalizeAfterImport();
}

void SetProfileInfo(AutofillProfile* profile,
                    const char* first_name,
                    const char* middle_name,
                    const char* last_name,
                    const char* email,
                    const char* company,
                    const char* address1,
                    const char* address2,
                    const char* city,
                    const char* state,
                    const char* zipcode,
                    const char* country,
                    const char* phone,
                    bool finalize,
                    structured_address::VerificationStatus status) {
  check_and_set(profile, NAME_FIRST, first_name, status);
  check_and_set(profile, NAME_MIDDLE, middle_name, status);
  check_and_set(profile, NAME_LAST, last_name, status);
  check_and_set(profile, EMAIL_ADDRESS, email, status);
  check_and_set(profile, COMPANY_NAME, company, status);
  check_and_set(profile, ADDRESS_HOME_LINE1, address1, status);
  check_and_set(profile, ADDRESS_HOME_LINE2, address2, status);
  check_and_set(profile, ADDRESS_HOME_CITY, city, status);
  check_and_set(profile, ADDRESS_HOME_STATE, state, status);
  check_and_set(profile, ADDRESS_HOME_ZIP, zipcode, status);
  check_and_set(profile, ADDRESS_HOME_COUNTRY, country, status);
  check_and_set(profile, PHONE_HOME_WHOLE_NUMBER, phone, status);
  if (finalize)
    profile->FinalizeAfterImport();
}

void SetProfileInfoWithGuid(AutofillProfile* profile,
                            const char* guid,
                            const char* first_name,
                            const char* middle_name,
                            const char* last_name,
                            const char* email,
                            const char* company,
                            const char* address1,
                            const char* address2,
                            const char* city,
                            const char* state,
                            const char* zipcode,
                            const char* country,
                            const char* phone,
                            bool finalize,
                            structured_address::VerificationStatus status) {
  if (guid)
    profile->set_guid(guid);
  SetProfileInfo(profile, first_name, middle_name, last_name, email, company,
                 address1, address2, city, state, zipcode, country, phone,
                 finalize, status);
}

void SetCreditCardInfo(CreditCard* credit_card,
                       const char* name_on_card,
                       const char* card_number,
                       const char* expiration_month,
                       const char* expiration_year,
                       const std::string& billing_address_id) {
  check_and_set(credit_card, CREDIT_CARD_NAME_FULL, name_on_card);
  check_and_set(credit_card, CREDIT_CARD_NUMBER, card_number);
  check_and_set(credit_card, CREDIT_CARD_EXP_MONTH, expiration_month);
  check_and_set(credit_card, CREDIT_CARD_EXP_4_DIGIT_YEAR, expiration_year);
  credit_card->set_billing_address_id(billing_address_id);
}

void DisableSystemServices(PrefService* prefs) {
  // Use a mock Keychain rather than the OS one to store credit card data.
  OSCryptMocker::SetUp();
}

void ReenableSystemServices() {
  OSCryptMocker::TearDown();
}

void SetServerCreditCards(AutofillTable* table,
                          const std::vector<CreditCard>& cards) {
  std::vector<CreditCard> as_masked_cards = cards;
  for (CreditCard& card : as_masked_cards) {
    card.set_record_type(CreditCard::MASKED_SERVER_CARD);
    card.SetNumber(card.LastFourDigits());
    card.SetNetworkForMaskedCard(card.network());
    card.set_instrument_id(card.instrument_id());
  }
  table->SetServerCreditCards(as_masked_cards);

  for (const CreditCard& card : cards) {
    if (card.record_type() != CreditCard::FULL_SERVER_CARD)
      continue;
    ASSERT_TRUE(table->UnmaskServerCreditCard(card, card.number()));
  }
}

void InitializePossibleTypesAndValidities(
    std::vector<ServerFieldTypeSet>& possible_field_types,
    std::vector<ServerFieldTypeValidityStatesMap>&
        possible_field_types_validities,
    const std::vector<ServerFieldType>& possible_types,
    const std::vector<AutofillDataModel::ValidityState>& validity_states) {
  possible_field_types.push_back(ServerFieldTypeSet());
  possible_field_types_validities.push_back(ServerFieldTypeValidityStatesMap());

  if (validity_states.empty()) {
    for (const auto& possible_type : possible_types) {
      possible_field_types.back().insert(possible_type);
      possible_field_types_validities.back()[possible_type].push_back(
          AutofillProfile::UNVALIDATED);
    }
    return;
  }

  ASSERT_FALSE(possible_types.empty());
  ASSERT_TRUE((possible_types.size() == validity_states.size()) ||
              (possible_types.size() == 1 && validity_states.size() > 1));

  ServerFieldType possible_type = possible_types[0];
  for (unsigned i = 0; i < validity_states.size(); ++i) {
    if (possible_types.size() == validity_states.size()) {
      possible_type = possible_types[i];
    }
    possible_field_types.back().insert(possible_type);
    possible_field_types_validities.back()[possible_type].push_back(
        validity_states[i]);
  }
}

void BasicFillUploadField(AutofillUploadContents::Field* field,
                          unsigned signature,
                          const char* name,
                          const char* control_type,
                          const char* autocomplete) {
  field->set_signature(signature);
  if (name)
    field->set_name(name);
  if (control_type)
    field->set_type(control_type);
  if (autocomplete)
    field->set_autocomplete(autocomplete);
}

void FillUploadField(AutofillUploadContents::Field* field,
                     unsigned signature,
                     const char* name,
                     const char* control_type,
                     const char* autocomplete,
                     unsigned autofill_type,
                     unsigned validity_state) {
  BasicFillUploadField(field, signature, name, control_type, autocomplete);

  field->add_autofill_type(autofill_type);

  auto* type_validities = field->add_autofill_type_validities();
  type_validities->set_type(autofill_type);
  type_validities->add_validity(validity_state);
}

void FillUploadField(AutofillUploadContents::Field* field,
                     unsigned signature,
                     const char* name,
                     const char* control_type,
                     const char* autocomplete,
                     const std::vector<unsigned>& autofill_types,
                     const std::vector<unsigned>& validity_states) {
  BasicFillUploadField(field, signature, name, control_type, autocomplete);

  for (unsigned i = 0; i < autofill_types.size(); ++i) {
    field->add_autofill_type(autofill_types[i]);

    auto* type_validities = field->add_autofill_type_validities();
    type_validities->set_type(autofill_types[i]);
    if (i < validity_states.size()) {
      type_validities->add_validity(validity_states[i]);
    } else {
      type_validities->add_validity(0);
    }
  }
}

void FillUploadField(AutofillUploadContents::Field* field,
                     unsigned signature,
                     const char* name,
                     const char* control_type,
                     const char* autocomplete,
                     unsigned autofill_type,
                     const std::vector<unsigned>& validity_states) {
  BasicFillUploadField(field, signature, name, control_type, autocomplete);

  field->add_autofill_type(autofill_type);
  auto* type_validities = field->add_autofill_type_validities();
  type_validities->set_type(autofill_type);
  for (unsigned i = 0; i < validity_states.size(); ++i)
    type_validities->add_validity(validity_states[i]);
}

void GenerateTestAutofillPopup(
    AutofillExternalDelegate* autofill_external_delegate) {
  int query_id = 1;
  FormData form;
  FormFieldData field;
  field.is_focusable = true;
  field.should_autocomplete = true;
  gfx::RectF bounds(100.f, 100.f);
  autofill_external_delegate->OnQuery(query_id, form, field, bounds);

  std::vector<Suggestion> suggestions;
  suggestions.push_back(Suggestion(u"Test suggestion"));
  autofill_external_delegate->OnSuggestionsReturned(
      query_id, suggestions, /*autoselect_first_suggestion=*/false);
}

std::string ObfuscatedCardDigitsAsUTF8(const std::string& str,
                                       int obfuscation_length) {
  return base::UTF16ToUTF8(internal::GetObfuscatedStringForCardDigits(
      base::ASCIIToUTF16(str), obfuscation_length));
}

std::string NextMonth() {
  base::Time::Exploded now;
  AutofillClock::Now().LocalExplode(&now);
  return base::StringPrintf("%02d", now.month % 12 + 1);
}
std::string LastYear() {
  base::Time::Exploded now;
  AutofillClock::Now().LocalExplode(&now);
  return base::NumberToString(now.year - 1);
}
std::string NextYear() {
  base::Time::Exploded now;
  AutofillClock::Now().LocalExplode(&now);
  return base::NumberToString(now.year + 1);
}
std::string TenYearsFromNow() {
  base::Time::Exploded now;
  AutofillClock::Now().LocalExplode(&now);
  return base::NumberToString(now.year + 10);
}

std::vector<FormSignature> GetEncodedSignatures(const FormStructure& form) {
  std::vector<FormSignature> signatures;
  signatures.push_back(form.form_signature());
  return signatures;
}

std::vector<FormSignature> GetEncodedSignatures(
    const std::vector<FormStructure*>& forms) {
  std::vector<FormSignature> all_signatures;
  for (const FormStructure* form : forms)
    all_signatures.push_back(form->form_signature());
  return all_signatures;
}

void AddFieldSuggestionToForm(
    const autofill::FormFieldData& field_data,
    ServerFieldType field_type,
    ::autofill::AutofillQueryResponse_FormSuggestion* form_suggestion) {
  auto* field_suggestion = form_suggestion->add_field_suggestions();
  field_suggestion->set_field_signature(
      CalculateFieldSignatureForField(field_data).value());
  field_suggestion->add_predictions()->set_type(field_type);
}

void AddFieldPredictionsToForm(
    const autofill::FormFieldData& field_data,
    const std::vector<int>& field_types,
    ::autofill::AutofillQueryResponse_FormSuggestion* form_suggestion) {
  std::vector<ServerFieldType> types;
  for (auto type : field_types) {
    types.emplace_back(ToSafeServerFieldType(type, UNKNOWN_TYPE));
  }
  AddFieldPredictionsToForm(field_data, types, form_suggestion);
}

void AddFieldPredictionsToForm(
    const autofill::FormFieldData& field_data,
    const std::vector<ServerFieldType>& field_types,
    ::autofill::AutofillQueryResponse_FormSuggestion* form_suggestion) {
  auto* field_suggestion = form_suggestion->add_field_suggestions();
  field_suggestion->set_field_signature(
      CalculateFieldSignatureForField(field_data).value());
  for (auto field_type : field_types) {
    AutofillQueryResponse_FormSuggestion_FieldSuggestion_FieldPrediction*
        prediction = field_suggestion->add_predictions();
    prediction->set_type(field_type);
  }
}

}  // namespace test
}  // namespace autofill
