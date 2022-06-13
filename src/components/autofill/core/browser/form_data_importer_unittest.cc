// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_data_importer.h"

#include <stddef.h>

#include <algorithm>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/guid.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "components/prefs/pref_service.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_database_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using base::UTF8ToUTF16;
using testing::_;
using testing::AtLeast;
using testing::Invoke;
using testing::Return;
using testing::SaveArg;
using testing::UnorderedElementsAre;
using testing::WithoutArgs;

namespace autofill {
namespace {

// Define values for the default address profile.
constexpr char kDefaultFullName[] = "Thomas Neo Anderson";
constexpr char kDefaultFirstName[] = "Thomas";
constexpr char kDefaultLastName[] = "Anderson";
constexpr char kDefaultMail[] = "theone@thematrix.org";
constexpr char kDefaultAddressLine1[] = "21 Laussat St";
constexpr char kDefaultStreetAddress[] = "21 Laussat St\\nApt 123";
constexpr char kDefaultZip[] = "94102";
constexpr char kDefaultCity[] = "Los Angeles";
constexpr char kDefaultDependentLocality[] = "Nob Hill";
constexpr char kDefaultState[] = "California";
constexpr char kDefaultPhone[] = "+16505550000";
constexpr char kDefaultPhoneAlternativeFormatting[] = "+1-650-555-0000";
constexpr char kDefaultPhoneAreaCode[] = "650";
constexpr char kDefaultPhonePrefix[] = "555";
constexpr char kDefaultPhoneSuffix[] = "0000";

// Define values for a second address profile.
constexpr char kSecondFirstName[] = "Bruce";
constexpr char kSecondLastName[] = "Wayne";
constexpr char kSecondMail[] = "wayne@bruce.org";
constexpr char kSecondAddressLine1[] = "23 Main St";
constexpr char kSecondZip[] = "94106";
constexpr char kSecondCity[] = "Los Angeles";
constexpr char kSecondDependentLocality[] = "Down Town";
constexpr char kSecondState[] = "California";
constexpr char kSecondPhone[] = "+16516661111";
constexpr char kSecondPhoneAreaCode[] = "651";
constexpr char kSecondPhonePrefix[] = "666";
constexpr char kSecondPhoneSuffix[] = "1111";

// Define values for a third address profile.
constexpr char kThirdFirstName[] = "Homer";
constexpr char kThirdLastName[] = "Simpson";
constexpr char kThirdMail[] = "donut@whatever.net";
constexpr char kThirdAddressLine1[] = "742 Evergreen Terrace";
constexpr char kThirdZip[] = "65619";
constexpr char kThirdCity[] = "Springfield";
constexpr char kThirdDependentLocality[] = "Down Town";
constexpr char kThirdState[] = "Oregon";
constexpr char kThirdPhone[] = "+18517772222";

constexpr char kDefaultCreditCardNumber[] = "4111 1111 1111 1111";

// For a given ServerFieldType |type| returns a pair of field name and label
// that should be parsed into this type by our field type parsers.
std::pair<std::string, std::string> GetLabelAndNameForType(
    ServerFieldType type) {
  static const std::map<ServerFieldType, std::pair<std::string, std::string>>
      name_type_map = {
          {NAME_FULL, {"Full Name:", "full_name"}},
          {NAME_FIRST, {"First Name:", "first_name"}},
          {NAME_MIDDLE, {"Middle Name", "middle_name"}},
          {NAME_LAST, {"Last Name:", "last_name"}},
          {EMAIL_ADDRESS, {"Email:", "email"}},
          {ADDRESS_HOME_LINE1, {"Address:", "address1"}},
          {ADDRESS_HOME_STREET_ADDRESS, {"Address:", "address"}},
          {ADDRESS_HOME_CITY, {"City:", "city"}},
          {ADDRESS_HOME_ZIP, {"Zip:", "zip"}},
          {ADDRESS_HOME_STATE, {"State:", "state"}},
          {ADDRESS_HOME_DEPENDENT_LOCALITY, {"Neighborhood:", "neighborhood"}},
          {ADDRESS_HOME_COUNTRY, {"Country:", "country"}},
          {PHONE_HOME_WHOLE_NUMBER, {"Phone:", "phone"}},
          {CREDIT_CARD_NUMBER, {"Credit Card Number:", "card_number"}},
      };
  auto it = name_type_map.find(type);
  if (it == name_type_map.end()) {
    NOTIMPLEMENTED() << " field name and label is missing for "
                     << AutofillType(type).ToString();
    return {std::string(), std::string()};
  }
  return it->second;
}

// Constructs a FormData instance for |url| from a vector of type value pairs
// that defines a sequence of fields and the filled values.
// The field names and labels for the different types are relieved from
// |GetLabelAndNameForType(type)|
FormData ConstructFormDateFromTypeValuePairs(
    std::vector<std::pair<ServerFieldType, std::string>> type_value_pairs,
    std::string url = "https://www.foo.com") {
  FormData form;
  form.url = GURL(url);

  FormFieldData field;
  for (auto type_value_pair : type_value_pairs) {
    ServerFieldType type = type_value_pair.first;
    std::string value = type_value_pair.second;
    std::pair<std::string, std::string> name_and_label =
        GetLabelAndNameForType(type);

    test::CreateTestFormField(
        name_and_label.first.c_str(), name_and_label.second.c_str(),
        value.c_str(),
        type == ADDRESS_HOME_STREET_ADDRESS ? "textarea" : "text", &field);
    form.fields.push_back(field);
  }

  return form;
}

// Constructs a FormStructure instance from a FormData instance and determines
// the heuristic types.
std::unique_ptr<FormStructure> ConstructFormStructureFromFormData(
    const FormData& form) {
  auto form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes(nullptr, nullptr);
  return form_structure;
}

// Constructs a FormStructure instance with fields and inserted values given by
// a vector of type and value pairs.
std::unique_ptr<FormStructure> ConstructFormStructureFromTypeValuePairs(
    std::vector<std::pair<ServerFieldType, std::string>> type_value_pairs,
    std::string url = "https://www.foo.com") {
  FormData form = ConstructFormDateFromTypeValuePairs(type_value_pairs, url);
  return ConstructFormStructureFromFormData(form);
}

// Construct and finalizes an AutofillProfile based on a vector of type and
// value pairs. The values are set as |VerificationStatus::kObserved| and the
// profile is finalizes in the end.
AutofillProfile ConstructProfileFromTypeValuePairs(
    std::vector<std::pair<ServerFieldType, std::string>> type_value_pairs) {
  AutofillProfile profile;
  for (const auto& type_value_pair : type_value_pairs) {
    profile.SetRawInfoWithVerificationStatus(
        type_value_pair.first, base::UTF8ToUTF16(type_value_pair.second),
        structured_address::VerificationStatus::kObserved);
  }
  if (!profile.FinalizeAfterImport())
    NOTREACHED();
  return profile;
}

// Returns a vector of ServerFieldType and value pairs used to construct the
// default AutofillProfile, or a FormStructure or FormData instance that carries
// that corresponding information.
std::vector<std::pair<ServerFieldType, std::string>>
GetDefaultProfileTypeValuePairs() {
  return {
      {NAME_FIRST, kDefaultFirstName},
      {NAME_LAST, kDefaultLastName},
      {EMAIL_ADDRESS, kDefaultMail},
      {PHONE_HOME_WHOLE_NUMBER, kDefaultPhone},
      {ADDRESS_HOME_LINE1, kDefaultAddressLine1},
      {ADDRESS_HOME_DEPENDENT_LOCALITY, kDefaultDependentLocality},
      {ADDRESS_HOME_CITY, kDefaultCity},
      {ADDRESS_HOME_STATE, kDefaultState},
      {ADDRESS_HOME_ZIP, kDefaultZip},
  };
}

// Same as |GetDefaultProfileTypeValuePairs()| but with the second profile
// information.
std::vector<std::pair<ServerFieldType, std::string>>
GetSecondProfileTypeValuePairs() {
  return {
      {NAME_FIRST, kSecondFirstName},
      {NAME_LAST, kSecondLastName},
      {EMAIL_ADDRESS, kSecondMail},
      {PHONE_HOME_WHOLE_NUMBER, kSecondPhone},
      {ADDRESS_HOME_LINE1, kSecondAddressLine1},
      {ADDRESS_HOME_DEPENDENT_LOCALITY, kSecondDependentLocality},
      {ADDRESS_HOME_CITY, kSecondCity},
      {ADDRESS_HOME_STATE, kSecondState},
      {ADDRESS_HOME_ZIP, kSecondZip},
  };
}

// Same as |GetDefaultProfileTypeValuePairs()| but with the third profile
// information.
std::vector<std::pair<ServerFieldType, std::string>>
GetThirdProfileTypeValuePairs() {
  return {
      {NAME_FIRST, kThirdFirstName},
      {NAME_LAST, kThirdLastName},
      {EMAIL_ADDRESS, kThirdMail},
      {PHONE_HOME_WHOLE_NUMBER, kThirdPhone},
      {ADDRESS_HOME_LINE1, kThirdAddressLine1},
      {ADDRESS_HOME_DEPENDENT_LOCALITY, kThirdDependentLocality},
      {ADDRESS_HOME_CITY, kThirdCity},
      {ADDRESS_HOME_STATE, kThirdState},
      {ADDRESS_HOME_ZIP, kThirdZip},
  };
}

// Returns the default AutofillProfile used in this test file.
AutofillProfile ConstructDefaultProfile() {
  return ConstructProfileFromTypeValuePairs(GetDefaultProfileTypeValuePairs());
}

// Returns the second AutofillProfile used in this test file.
AutofillProfile ConstructSecondProfile() {
  return ConstructProfileFromTypeValuePairs(GetSecondProfileTypeValuePairs());
}

// Returns the third AutofillProfile used in this test file.
AutofillProfile ConstructThirdProfile() {
  return ConstructProfileFromTypeValuePairs(GetThirdProfileTypeValuePairs());
}

// Returns a form with the default profile. The AutofillProfile that is imported
// from this form should be similar to the profile create by calling
// |ConstructDefaultProfile()|
std::unique_ptr<FormStructure> ConstructDefaultProfileFormStructure() {
  return ConstructFormStructureFromTypeValuePairs(
      GetDefaultProfileTypeValuePairs());
}

// Same as |ConstructDefaultFormStructure()| but for the second profile.
std::unique_ptr<FormStructure> ConstructSecondProfileFormStructure() {
  return ConstructFormStructureFromTypeValuePairs(
      GetSecondProfileTypeValuePairs());
}

// Same as |ConstructDefaultFormStructure()| but for the third profile.
std::unique_ptr<FormStructure> ConstructThirdProfileFormStructure() {
  return ConstructFormStructureFromTypeValuePairs(
      GetThirdProfileTypeValuePairs());
}

// Constructs a |FormData| instance that carries the information of the default
// profile.
FormData ConstructDefaultFormData() {
  return ConstructFormDateFromTypeValuePairs(GetDefaultProfileTypeValuePairs());
}

ACTION_P(QuitMessageLoop, loop) {
  loop->Quit();
}

enum UserMode { USER_MODE_NORMAL, USER_MODE_INCOGNITO };

class PersonalDataLoadedObserverMock : public PersonalDataManagerObserver {
 public:
  PersonalDataLoadedObserverMock() = default;
  ~PersonalDataLoadedObserverMock() override = default;

  MOCK_METHOD(void, OnPersonalDataChanged, (), (override));
  MOCK_METHOD(void, OnPersonalDataFinishedProfileTasks, (), (override));
};

template <typename T>
bool CompareElements(T* a, T* b) {
  return a->Compare(*b) < 0;
}

template <typename T>
bool ElementsEqual(T* a, T* b) {
  return a->Compare(*b) == 0;
}

// Verifies that two vectors have the same elements (according to T::Compare)
// while ignoring order. This is useful because multiple profiles or credit
// cards that are added to the SQLite DB within the same second will be returned
// in GUID (aka random) order.
template <typename T>
void ExpectSameElements(const std::vector<T*>& expectations,
                        const std::vector<T*>& results) {
  ASSERT_EQ(expectations.size(), results.size());

  std::vector<T*> expectations_copy = expectations;
  std::sort(expectations_copy.begin(), expectations_copy.end(),
            CompareElements<T>);
  std::vector<T*> results_copy = results;
  std::sort(results_copy.begin(), results_copy.end(), CompareElements<T>);

  EXPECT_EQ(std::mismatch(results_copy.begin(), results_copy.end(),
                          expectations_copy.begin(), ElementsEqual<T>)
                .first,
            results_copy.end());
}

}  // anonymous namespace

class FormDataImporterTestBase {
 protected:
  FormDataImporterTestBase() : autofill_table_(nullptr) {}

  void ResetPersonalDataManager(UserMode user_mode) {
    personal_data_manager_ = std::make_unique<PersonalDataManager>("en", "US");
    personal_data_manager_->Init(
        scoped_refptr<AutofillWebDataService>(autofill_database_service_),
        /*account_database=*/nullptr,
        /*pref_service=*/prefs_.get(),
        /*local_state=*/prefs_.get(),
        /*identity_manager=*/nullptr,
        /*client_profile_validator=*/nullptr,
        /*history_service=*/nullptr,
        /*strike_database=*/nullptr,
        /*image_fetcher=*/nullptr,
        /*is_off_the_record=*/(user_mode == USER_MODE_INCOGNITO));
    personal_data_manager_->AddObserver(&personal_data_observer_);
    personal_data_manager_->OnSyncServiceInitialized(nullptr);

    WaitForOnPersonalDataChanged();
  }

  // Helper method that will add credit card fields in |form|, according to the
  // specified values. If a value is nullptr, the corresponding field won't get
  // added (empty string will add a field with an empty string as the value).
  void AddFullCreditCardForm(FormData* form,
                             const char* name,
                             const char* number,
                             const char* month,
                             const char* year) {
    FormFieldData field;
    if (name) {
      test::CreateTestFormField("Name on card:", "name_on_card", name, "text",
                                &field);
      form->fields.push_back(field);
    }
    if (number) {
      test::CreateTestFormField("Card Number:", "card_number", number, "text",
                                &field);
      form->fields.push_back(field);
    }
    if (month) {
      test::CreateTestFormField("Exp Month:", "exp_month", month, "text",
                                &field);
      form->fields.push_back(field);
    }
    if (year) {
      test::CreateTestFormField("Exp Year:", "exp_year", year, "text", &field);
      form->fields.push_back(field);
    }
  }

  // Helper methods that simply forward the call to the private member (to avoid
  // having to friend every test that needs to access the private
  // PersonalDataManager::ImportAddressProfile or ImportCreditCard).
  void ImportAddressProfiles(bool extraction_successful,
                             const FormStructure& form,
                             bool skip_waiting_on_pdm = false,
                             bool allow_save_prompts = true) {
    // This parameter has no effect unless save prompts for addresses are
    // enabled.
    allow_save_prompts =
        allow_save_prompts || !base::FeatureList::IsEnabled(
                                  features::kAutofillAddressProfileSavePrompt);

    std::vector<FormDataImporter::AddressProfileImportCandidate>
        address_profile_import_candidates;

    EXPECT_EQ(extraction_successful,
              form_data_importer_->ImportAddressProfiles(
                  form, address_profile_import_candidates));

    if (!extraction_successful) {
      EXPECT_FALSE(form_data_importer_->ProcessAddressProfileImportCandidates(
          address_profile_import_candidates, allow_save_prompts));
      return;
    }

    if (skip_waiting_on_pdm) {
      EXPECT_EQ(form_data_importer_->ProcessAddressProfileImportCandidates(
                    address_profile_import_candidates, allow_save_prompts),
                allow_save_prompts);
      return;
    }

    base::RunLoop run_loop;
    EXPECT_CALL(personal_data_observer_, OnPersonalDataFinishedProfileTasks())
        .WillOnce(QuitMessageLoop(&run_loop));
    EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged()).Times(1);
    EXPECT_EQ(form_data_importer_->ProcessAddressProfileImportCandidates(
                  address_profile_import_candidates, allow_save_prompts),
              allow_save_prompts);
    run_loop.Run();
  }

  // Verifies that the stored profiles in the PersonalDataManager equal
  // |expected_profiles| with respect to |AutofillProfile::Compare|.
  // Note, that order is taken into account.
  void VerifyExpectationForImportedAddressProfiles(
      const std::vector<AutofillProfile>& expected_profiles) {
    std::vector<AutofillProfile> imported_profiles;
    size_t expected_profile_index = 0;
    for (const auto* profile : personal_data_manager_->GetProfiles()) {
      ASSERT_LT(expected_profile_index, expected_profiles.size());
      profile->Compare(expected_profiles[expected_profile_index++]);
    }
  }

  // Convenience wrapper that calls |FormDataImporter::ImportFormData()| and
  // subsequetly processes the candidates for address profile import.
  // Returns the result of |FormDataImporter::ImportFormData()|.
  bool ImportFormDataAndProcessAddressCandidates(
      const FormStructure& form,
      bool profile_autofill_enabled,
      bool credit_card_autofill_enabled,
      bool should_return_local_card,
      std::unique_ptr<CreditCard>* imported_credit_card,
      absl::optional<std::string>* imported_upi_id) {
    std::vector<FormDataImporter::AddressProfileImportCandidate>
        address_profile_import_candidates;

    bool result = form_data_importer_->ImportFormData(
        form, profile_autofill_enabled, credit_card_autofill_enabled,
        should_return_local_card, imported_credit_card,
        address_profile_import_candidates, imported_upi_id);

    form_data_importer_->ProcessAddressProfileImportCandidates(
        address_profile_import_candidates);

    return result;
  }

  void ImportAddressProfilesAndVerifyExpectation(
      const FormStructure& form,
      const std::vector<AutofillProfile>& expected_profiles) {
    ImportAddressProfiles(/*extraction_successful=*/!expected_profiles.empty(),
                          form);
    VerifyExpectationForImportedAddressProfiles(expected_profiles);
  }

  void ImportAddressProfileAndVerifyImportOfDefaultProfile(
      const FormStructure& form) {
    ImportAddressProfilesAndVerifyExpectation(form,
                                              {ConstructDefaultProfile()});
  }

  void ImportAddressProfileAndVerifyImportOfNoProfile(
      const FormStructure& form) {
    ImportAddressProfilesAndVerifyExpectation(form, {});
  }

  bool ImportCreditCard(const FormStructure& form,
                        bool should_return_local_card,
                        std::unique_ptr<CreditCard>* imported_credit_card) {
    return form_data_importer_->ImportCreditCard(form, should_return_local_card,
                                                 imported_credit_card);
  }

  void SubmitFormAndExpectImportedCardWithData(const FormData& form,
                                               const char* exp_name,
                                               const char* exp_cc_num,
                                               const char* exp_cc_month,
                                               const char* exp_cc_year) {
    FormStructure form_structure(form);
    form_structure.DetermineHeuristicTypes(nullptr, nullptr);
    std::unique_ptr<CreditCard> imported_credit_card;
    EXPECT_TRUE(ImportCreditCard(form_structure, false, &imported_credit_card));
    ASSERT_TRUE(imported_credit_card);
    personal_data_manager_->OnAcceptedLocalCreditCardSave(
        *imported_credit_card);

    WaitForOnPersonalDataChanged();
    CreditCard expected(base::GenerateGUID(), test::kEmptyOrigin);
    test::SetCreditCardInfo(&expected, exp_name, exp_cc_num, exp_cc_month,
                            exp_cc_year, "");
    const std::vector<CreditCard*>& results =
        personal_data_manager_->GetCreditCards();
    ASSERT_EQ(1U, results.size());
    EXPECT_EQ(0, expected.Compare(*results[0]));
  }

  void WaitForOnPersonalDataChanged() {
    base::RunLoop run_loop;
    EXPECT_CALL(personal_data_observer_, OnPersonalDataFinishedProfileTasks())
        .WillOnce(QuitMessageLoop(&run_loop));
    EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
        .Times(testing::AnyNumber());
    run_loop.Run();
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI};
  std::unique_ptr<PrefService> prefs_;
  scoped_refptr<AutofillWebDataService> autofill_database_service_;
  scoped_refptr<WebDatabaseService> web_database_;
  raw_ptr<AutofillTable> autofill_table_;  // weak ref
  PersonalDataLoadedObserverMock personal_data_observer_;
  std::unique_ptr<TestAutofillClient> autofill_client_;
  std::unique_ptr<PersonalDataManager> personal_data_manager_;
  std::unique_ptr<FormDataImporter> form_data_importer_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/1103421): Clean legacy implementation once structured names
// are fully launched. Here, the changes applied in CL 2339350 must be reverted
// by removing the parameterization.
class FormDataImporterTest
    : public FormDataImporterTestBase,
      public testing::Test,
      public testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 protected:
  bool StructuredNames() const { return structured_names_enabled_; }
  bool StructuredAddresses() const { return structured_addresses_enabled_; }

 private:
  void SetUp() override {
    InitializeFeatures();
    OSCryptMocker::SetUp();
    prefs_ = test::PrefServiceForTesting();
    base::FilePath path(WebDatabase::kInMemoryPath);
    web_database_ =
        new WebDatabaseService(path, base::ThreadTaskRunnerHandle::Get(),
                               base::ThreadTaskRunnerHandle::Get());

    // Hacky: hold onto a pointer but pass ownership.
    autofill_table_ = new AutofillTable;
    web_database_->AddTable(std::unique_ptr<WebDatabaseTable>(autofill_table_));
    web_database_->LoadDatabase();
    autofill_database_service_ = new AutofillWebDataService(
        web_database_, base::ThreadTaskRunnerHandle::Get(),
        base::ThreadTaskRunnerHandle::Get());
    autofill_database_service_->Init(base::NullCallback());

    autofill_client_ = std::make_unique<TestAutofillClient>();

    test::DisableSystemServices(prefs_.get());
    ResetPersonalDataManager(USER_MODE_NORMAL);

    form_data_importer_ =
        std::make_unique<FormDataImporter>(autofill_client_.get(),
                                           /*payments::PaymentsClient=*/nullptr,
                                           personal_data_manager_.get(), "en");

    // Reset the deduping pref to its default value.
    personal_data_manager_->pref_service_->SetInteger(
        prefs::kAutofillLastVersionDeduped, 0);
  }

  void TearDown() override {
    // Order of destruction is important as BrowserAutofillManager relies on
    // PersonalDataManager to be around when it gets destroyed.
    test::ReenableSystemServices();
    OSCryptMocker::TearDown();
  }

  void InitializeFeatures() {
    structured_names_enabled_ = std::get<0>(GetParam());
    structured_addresses_enabled_ = std::get<1>(GetParam());
    support_for_apartment_numbers_ = std::get<2>(GetParam());

    std::vector<base::Feature> enabled_features;
    std::vector<base::Feature> disabled_features;

    if (structured_names_enabled_) {
      enabled_features.push_back(
          features::kAutofillEnableSupportForMoreStructureInNames);
    } else {
      disabled_features.push_back(
          features::kAutofillEnableSupportForMoreStructureInNames);
    }

    if (structured_addresses_enabled_) {
      enabled_features.push_back(
          features::kAutofillEnableSupportForMoreStructureInAddresses);
    } else {
      disabled_features.push_back(
          features::kAutofillEnableSupportForMoreStructureInAddresses);
    }

    if (support_for_apartment_numbers_) {
      enabled_features.push_back(
          features::kAutofillEnableSupportForApartmentNumbers);
    } else {
      disabled_features.push_back(
          features::kAutofillEnableSupportForApartmentNumbers);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  bool structured_names_enabled_;
  bool structured_addresses_enabled_;
  bool support_for_apartment_numbers_;
};

// ImportAddressProfiles tests.
TEST_P(FormDataImporterTest, ImportStructuredNameProfile) {
  base::test::ScopedFeatureList structured_addresses_feature;
  structured_addresses_feature.InitAndEnableFeature(
      features::kAutofillEnableSupportForMoreStructureInAddresses);

  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name:", "name", "Pablo Diego Ruiz y Picasso",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "theprez@gmail.com", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "21 Laussat St", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  ImportAddressProfiles(/*extraction_successful=*/true, form_structure);

  const std::vector<AutofillProfile*>& results =
      personal_data_manager_->GetProfiles();
  ASSERT_EQ(1U, results.size());

  EXPECT_EQ(results[0]->GetRawInfo(ADDRESS_HOME_HOUSE_NUMBER), u"21");
  EXPECT_EQ(results[0]->GetRawInfo(ADDRESS_HOME_STREET_NAME), u"Laussat St");
  EXPECT_EQ(results[0]->GetRawInfo(ADDRESS_HOME_STREET_ADDRESS),
            u"21 Laussat St");

  EXPECT_EQ(results[0]->GetVerificationStatus(ADDRESS_HOME_HOUSE_NUMBER),
            structured_address::VerificationStatus::kParsed);
  EXPECT_EQ(results[0]->GetVerificationStatus(ADDRESS_HOME_STREET_NAME),
            structured_address::VerificationStatus::kParsed);
  EXPECT_EQ(results[0]->GetVerificationStatus(ADDRESS_HOME_STREET_ADDRESS),
            structured_address::VerificationStatus::kObserved);
}

TEST_P(FormDataImporterTest,
       ImportStructuredAddressProfile_StreetNameAndHouseNumber) {
  base::test::ScopedFeatureList structured_addresses_feature;
  structured_addresses_feature.InitAndEnableFeature(
      features::kAutofillEnableSupportForMoreStructureInAddresses);

  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name:", "name", "Pablo Diego Ruiz y Picasso",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "theprez@gmail.com", "text",
                            &field);
  form.fields.push_back(field);
  form.fields.push_back(field);
  test::CreateTestFormField("Street name:", "street_name", "Laussat St", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("House number:", "house_number", "21", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  ImportAddressProfiles(/*extraction_successful=*/true, form_structure);

  const std::vector<AutofillProfile*>& results =
      personal_data_manager_->GetProfiles();
  ASSERT_EQ(1U, results.size());

  EXPECT_EQ(results[0]->GetRawInfo(ADDRESS_HOME_HOUSE_NUMBER), u"21");
  EXPECT_EQ(results[0]->GetRawInfo(ADDRESS_HOME_STREET_NAME), u"Laussat St");
  EXPECT_EQ(results[0]->GetRawInfo(ADDRESS_HOME_STREET_ADDRESS),
            u"21 Laussat St");

  EXPECT_EQ(results[0]->GetVerificationStatus(ADDRESS_HOME_HOUSE_NUMBER),
            structured_address::VerificationStatus::kObserved);
  EXPECT_EQ(results[0]->GetVerificationStatus(ADDRESS_HOME_STREET_NAME),
            structured_address::VerificationStatus::kObserved);
  EXPECT_EQ(results[0]->GetVerificationStatus(ADDRESS_HOME_STREET_ADDRESS),
            structured_address::VerificationStatus::kFormatted);
}

TEST_P(
    FormDataImporterTest,
    ImportStructuredAddressProfile_StreetNameAndHouseNumberAndApartmentNumber) {
  // This test is only applicable for enabled structured addresses and support
  // for apartment numbers.
  if (!base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForMoreStructureInAddresses) ||
      !base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForApartmentNumbers)) {
    return;
  }
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name:", "name", "Pablo Diego Ruiz y Picasso",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "theprez@gmail.com", "text",
                            &field);
  form.fields.push_back(field);
  form.fields.push_back(field);
  test::CreateTestFormField("Street name:", "street_name", "Laussat St", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("House number:", "house_number", "21", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Apartment", "apartment", "101", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  ImportAddressProfiles(/*extraction_successful=*/true, form_structure);

  const std::vector<AutofillProfile*>& results =
      personal_data_manager_->GetProfiles();
  ASSERT_EQ(1U, results.size());

  EXPECT_EQ(results[0]->GetRawInfo(ADDRESS_HOME_HOUSE_NUMBER), u"21");
  EXPECT_EQ(results[0]->GetRawInfo(ADDRESS_HOME_STREET_NAME), u"Laussat St");
  EXPECT_EQ(results[0]->GetRawInfo(ADDRESS_HOME_STREET_ADDRESS),
            u"21 Laussat St APT 101");
  EXPECT_EQ(results[0]->GetRawInfo(ADDRESS_HOME_APT_NUM), u"101");
  EXPECT_EQ(results[0]->GetVerificationStatus(ADDRESS_HOME_HOUSE_NUMBER),
            structured_address::VerificationStatus::kObserved);
  EXPECT_EQ(results[0]->GetVerificationStatus(ADDRESS_HOME_STREET_NAME),
            structured_address::VerificationStatus::kObserved);
  EXPECT_EQ(results[0]->GetVerificationStatus(ADDRESS_HOME_STREET_ADDRESS),
            structured_address::VerificationStatus::kFormatted);
}

TEST_P(FormDataImporterTest,
       ImportStructuredAddressProfile_GermanStreetNameAndHouseNumber) {
  // This test is only applicable if structured addresses are enabled.
  if (!StructuredAddresses())
    return;

  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name:", "name", "Pablo Diego Ruiz y Picasso",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "theprez@gmail.com", "text",
                            &field);
  form.fields.push_back(field);
  form.fields.push_back(field);
  test::CreateTestFormField("Street name:", "street_name", "Hermann Strasse",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("House number:", "house_number", "23", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "Munich", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Country:", "country", "Germany", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "80992", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  ImportAddressProfiles(/*extraction_successful=*/true, form_structure);

  const std::vector<AutofillProfile*>& results =
      personal_data_manager_->GetProfiles();
  ASSERT_EQ(1U, results.size());

  EXPECT_EQ(results[0]->GetRawInfo(ADDRESS_HOME_HOUSE_NUMBER), u"23");
  EXPECT_EQ(results[0]->GetRawInfo(ADDRESS_HOME_STREET_NAME),
            u"Hermann Strasse");
  EXPECT_EQ(results[0]->GetRawInfo(ADDRESS_HOME_STREET_ADDRESS),
            u"Hermann Strasse 23");

  EXPECT_EQ(results[0]->GetVerificationStatus(ADDRESS_HOME_HOUSE_NUMBER),
            structured_address::VerificationStatus::kObserved);
  EXPECT_EQ(results[0]->GetVerificationStatus(ADDRESS_HOME_STREET_NAME),
            structured_address::VerificationStatus::kObserved);
  EXPECT_EQ(results[0]->GetVerificationStatus(ADDRESS_HOME_STREET_ADDRESS),
            structured_address::VerificationStatus::kFormatted);
}

// ImportAddressProfiles tests.
TEST_P(FormDataImporterTest, ImportStructuredNameAddressProfile) {
  base::test::ScopedFeatureList structured_addresses_feature;
  structured_addresses_feature.InitAndEnableFeature(
      features::kAutofillEnableSupportForMoreStructureInNames);

  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name:", "name", "Pablo Diego Ruiz y Picasso",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "theprez@gmail.com", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "21 Laussat St", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  ImportAddressProfiles(/*extraction_successful=*/true, form_structure);

  const std::vector<AutofillProfile*>& results =
      personal_data_manager_->GetProfiles();
  ASSERT_EQ(1U, results.size());

  EXPECT_EQ(results[0]->GetRawInfo(NAME_FULL), u"Pablo Diego Ruiz y Picasso");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_FIRST), u"Pablo Diego");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_MIDDLE), u"");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_LAST), u"Ruiz y Picasso");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_LAST_FIRST), u"Ruiz");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_LAST_CONJUNCTION), u"y");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_LAST_SECOND), u"Picasso");
}

TEST_P(FormDataImporterTest, ImportAddressProfiles) {
  base::test::ScopedFeatureList dependent_locality_feature;
  dependent_locality_feature.InitAndEnableFeature(
      features::kAutofillEnableDependentLocalityParsing);

  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultProfileFormStructure();
  ImportAddressProfileAndVerifyImportOfDefaultProfile(*form_structure);
}

TEST_P(FormDataImporterTest, ImportSecondAddressProfiles) {
  base::test::ScopedFeatureList dependent_locality_feature;
  dependent_locality_feature.InitAndEnableFeature(
      features::kAutofillEnableDependentLocalityParsing);

  std::unique_ptr<FormStructure> form_structure =
      ConstructSecondProfileFormStructure();
  ImportAddressProfilesAndVerifyExpectation(*form_structure,
                                            {ConstructSecondProfile()});
}

TEST_P(FormDataImporterTest, ImportThirdAddressProfiles) {
  base::test::ScopedFeatureList dependent_locality_feature;
  dependent_locality_feature.InitAndEnableFeature(
      features::kAutofillEnableDependentLocalityParsing);

  std::unique_ptr<FormStructure> form_structure =
      ConstructThirdProfileFormStructure();
  ImportAddressProfilesAndVerifyExpectation(*form_structure,
                                            {ConstructThirdProfile()});
}

// Test that the storage is prevented if the structured address prompt feature
// is enabled, but address prompts are not allowed.
TEST_P(FormDataImporterTest, ImportAddressProfiles_DontAllowPrompt) {
  base::test::ScopedFeatureList save_prompt_feature;
  save_prompt_feature.InitAndEnableFeature(
      features::kAutofillAddressProfileSavePrompt);

  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultProfileFormStructure();
  ImportAddressProfiles(/*extraction_successful=*/true, *form_structure,
                        /*skip_waiting_on_pdm=*/true,
                        /*allow_save_prompts=*/false);
  VerifyExpectationForImportedAddressProfiles({});

  save_prompt_feature.Reset();
  save_prompt_feature.InitAndDisableFeature(
      features::kAutofillAddressProfileSavePrompt);

  // Verify that the behavior changes when prompts are disabled.
  ImportAddressProfiles(/*extraction_successful=*/true, *form_structure,
                        /*skip_waiting_on_pdm=*/false,
                        /*allow_save_prompts=*/false);
  VerifyExpectationForImportedAddressProfiles({ConstructDefaultProfile()});
}

// Tests that even if the autocomplete prevents filling, it does not prevent
// import. For now, this is limited to the field with the signature 2281611779.
TEST_P(FormDataImporterTest, ImportAddressProfilesDespiteAutocomplete) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAutofillIgnoreAutocompleteForImport);

  FormData form = ConstructDefaultFormData();
  // Set the autocomplete attribute of the Address field to 'none' and set the
  // label to match the form that field that is covered by the special case that
  // is triggered by the field signature.
  ASSERT_EQ(form.fields[4].label, u"Address:");
  form.fields[4].autocomplete_attribute = "none";
  form.fields[4].name = u"checkout[shipping_address][address1]";

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  // Verify that the field signature matches the one for which the special case
  // is defined.
  ASSERT_EQ(form_structure->field(4)->GetFieldSignature(),
            FieldSignature(2281611779));
  // Verify that there is actually no storage type assigned due to the
  // autocomplete='none' attribute.
  ASSERT_EQ(form_structure->field(4)->Type().GetStorableType(), UNKNOWN_TYPE);

  // Check that the import works as expected.
  ImportAddressProfileAndVerifyImportOfDefaultProfile(*form_structure);
}

TEST_P(FormDataImporterTest, ImportAddressProfileFromUnifiedSection) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultProfileFormStructure();

  // Assign the address field another section than the other fields.
  form_structure->field(4)->section = "another_section";

  ImportAddressProfileAndVerifyImportOfDefaultProfile(*form_structure);
}

TEST_P(FormDataImporterTest, ImportAddressProfiles_BadEmail) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultProfileFormStructure();

  // Change the value of the email field.
  ASSERT_EQ(form_structure->field(2)->Type().GetStorableType(), EMAIL_ADDRESS);
  form_structure->field(2)->value = u"bogus";

  // Verify that there was no import.
  ImportAddressProfileAndVerifyImportOfNoProfile(*form_structure);
}

// Tests that a 'confirm email' field does not block profile import.
TEST_P(FormDataImporterTest, ImportAddressProfiles_TwoEmails) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(
          {{NAME_FIRST, kDefaultFirstName},
           {NAME_LAST, kDefaultLastName},
           {EMAIL_ADDRESS, kDefaultMail},
           // Add two email fields with the same value.
           {EMAIL_ADDRESS, kDefaultMail},
           {PHONE_HOME_WHOLE_NUMBER, kDefaultPhone},
           {ADDRESS_HOME_LINE1, kDefaultAddressLine1},
           {ADDRESS_HOME_DEPENDENT_LOCALITY, kDefaultDependentLocality},
           {ADDRESS_HOME_CITY, kDefaultCity},
           {ADDRESS_HOME_STATE, kDefaultState},
           {ADDRESS_HOME_ZIP, kDefaultZip}});

  ImportAddressProfileAndVerifyImportOfDefaultProfile(*form_structure);
}

// Tests two email fields containing different values blocks profile import.
TEST_P(FormDataImporterTest, ImportAddressProfiles_TwoDifferentEmails) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(
          {{NAME_FIRST, kDefaultFirstName},
           {NAME_LAST, kDefaultLastName},
           {EMAIL_ADDRESS, kDefaultMail},
           // Add two email fields with different values.
           {EMAIL_ADDRESS, "another@mail.com"},
           {PHONE_HOME_WHOLE_NUMBER, kDefaultPhone},
           {ADDRESS_HOME_LINE1, kDefaultAddressLine1},
           {ADDRESS_HOME_DEPENDENT_LOCALITY, kDefaultDependentLocality},
           {ADDRESS_HOME_CITY, kDefaultCity},
           {ADDRESS_HOME_STATE, kDefaultState},
           {ADDRESS_HOME_ZIP, kDefaultZip}});

  ImportAddressProfileAndVerifyImportOfNoProfile(*form_structure);
}

// Tests that multiple phone numbers do not block profile import and the first
// one is saved.
TEST_P(FormDataImporterTest, ImportAddressProfiles_MultiplePhoneNumbers) {
  base::test::ScopedFeatureList enable_import_when_multiple_phones_feature;
  enable_import_when_multiple_phones_feature.InitAndEnableFeature(
      features::kAutofillEnableImportWhenMultiplePhoneNumbers);

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(
          {{NAME_FIRST, kDefaultFirstName},
           {NAME_LAST, kDefaultLastName},
           {EMAIL_ADDRESS, kDefaultMail},
           {PHONE_HOME_WHOLE_NUMBER, kDefaultPhone},
           // Add a second phone field with a different number.
           {PHONE_HOME_WHOLE_NUMBER, kSecondPhone},
           {ADDRESS_HOME_LINE1, kDefaultAddressLine1},
           {ADDRESS_HOME_DEPENDENT_LOCALITY, kDefaultDependentLocality},
           {ADDRESS_HOME_CITY, kDefaultCity},
           {ADDRESS_HOME_STATE, kDefaultState},
           {ADDRESS_HOME_ZIP, kDefaultZip}});

  ImportAddressProfileAndVerifyImportOfDefaultProfile(*form_structure);
}

// Tests that multiple phone numbers do not block profile import and the first
// one is saved.
TEST_P(FormDataImporterTest,
       ImportAddressProfiles_MultiplePhoneNumbersSplitAcrossMultipleFields) {
  base::test::ScopedFeatureList enable_import_when_multiple_phones_feature;
  enable_import_when_multiple_phones_feature.InitAndEnableFeature(
      features::kAutofillEnableImportWhenMultiplePhoneNumbers);

  FormData form_data = ConstructFormDateFromTypeValuePairs(
      {{NAME_FIRST, kDefaultFirstName},
       {NAME_LAST, kDefaultLastName},
       {EMAIL_ADDRESS, kDefaultMail},
       // Add six phone number fields.
       {PHONE_HOME_WHOLE_NUMBER, kDefaultPhoneAreaCode},
       {PHONE_HOME_WHOLE_NUMBER, kDefaultPhonePrefix},
       {PHONE_HOME_WHOLE_NUMBER, kDefaultPhoneSuffix},
       {PHONE_HOME_WHOLE_NUMBER, kSecondPhoneAreaCode},
       {PHONE_HOME_WHOLE_NUMBER, kSecondPhonePrefix},
       {PHONE_HOME_WHOLE_NUMBER, kSecondPhoneSuffix},
       {ADDRESS_HOME_LINE1, kDefaultAddressLine1},
       {ADDRESS_HOME_DEPENDENT_LOCALITY, kDefaultDependentLocality},
       {ADDRESS_HOME_CITY, kDefaultCity},
       {ADDRESS_HOME_STATE, kDefaultState},
       {ADDRESS_HOME_ZIP, kDefaultZip}});

  form_data.fields[3].max_length = 3;
  form_data.fields[4].max_length = 3;
  form_data.fields[5].max_length = 4;
  form_data.fields[6].max_length = 3;
  form_data.fields[7].max_length = 3;
  form_data.fields[8].max_length = 4;

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form_data);
  ImportAddressProfileAndVerifyImportOfDefaultProfile(*form_structure);
}

// Tests that not enough filled fields will result in not importing an address.
TEST_P(FormDataImporterTest, ImportAddressProfiles_NotEnoughFilledFields) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(
          {{NAME_FIRST, kDefaultFirstName},
           {NAME_LAST, kDefaultLastName},
           {CREDIT_CARD_NUMBER, kDefaultCreditCardNumber}});

  ImportAddressProfileAndVerifyImportOfNoProfile(*form_structure);
  // Also verify that there was no import of a credit card.
  ASSERT_EQ(0U, personal_data_manager_->GetCreditCards().size());
}

TEST_P(FormDataImporterTest, ImportAddressProfiles_MinimumAddressUSA) {
  std::vector<std::pair<ServerFieldType, std::string>> type_value_pairs({
      {NAME_FULL, kDefaultFullName},
      {ADDRESS_HOME_LINE1, kDefaultAddressLine1},
      {ADDRESS_HOME_CITY, kDefaultCity},
      {ADDRESS_HOME_STATE, kDefaultState},
      {ADDRESS_HOME_ZIP, kDefaultZip},
      {ADDRESS_HOME_COUNTRY, "US"},
  });

  AutofillProfile profile =
      ConstructProfileFromTypeValuePairs(type_value_pairs);

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(type_value_pairs);
  ImportAddressProfilesAndVerifyExpectation(*form_structure, {profile});
}

TEST_P(FormDataImporterTest, ImportAddressProfiles_MinimumAddressGB) {
  std::vector<std::pair<ServerFieldType, std::string>> type_value_pairs({
      {NAME_FULL, kDefaultFullName},
      {ADDRESS_HOME_LINE1, kDefaultAddressLine1},
      {ADDRESS_HOME_CITY, kDefaultCity},
      {ADDRESS_HOME_ZIP, kDefaultZip},
      {ADDRESS_HOME_COUNTRY, "GB"},
  });

  AutofillProfile profile =
      ConstructProfileFromTypeValuePairs(type_value_pairs);

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(type_value_pairs);
  ImportAddressProfilesAndVerifyExpectation(*form_structure, {profile});
}

TEST_P(FormDataImporterTest, ImportAddressProfiles_MinimumAddressGI) {
  std::vector<std::pair<ServerFieldType, std::string>> type_value_pairs({
      {NAME_FULL, kDefaultFullName},
      {ADDRESS_HOME_LINE1, kDefaultAddressLine1},
      {ADDRESS_HOME_COUNTRY, "GI"},
  });

  AutofillProfile profile =
      ConstructProfileFromTypeValuePairs(type_value_pairs);

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(type_value_pairs);
  ImportAddressProfilesAndVerifyExpectation(*form_structure, {profile});
}

TEST_P(FormDataImporterTest,
       ImportAddressProfiles_PhoneNumberSplitAcrossMultipleFields) {
  FormData form_data = ConstructFormDateFromTypeValuePairs(
      {{NAME_FIRST, kDefaultFirstName},
       {NAME_LAST, kDefaultLastName},
       {EMAIL_ADDRESS, kDefaultMail},
       // Add three phone number fields.
       {PHONE_HOME_WHOLE_NUMBER, kDefaultPhoneAreaCode},
       {PHONE_HOME_WHOLE_NUMBER, kDefaultPhonePrefix},
       {PHONE_HOME_WHOLE_NUMBER, kDefaultPhoneSuffix},
       {ADDRESS_HOME_LINE1, kDefaultAddressLine1},
       {ADDRESS_HOME_DEPENDENT_LOCALITY, kDefaultDependentLocality},
       {ADDRESS_HOME_CITY, kDefaultCity},
       {ADDRESS_HOME_STATE, kDefaultState},
       {ADDRESS_HOME_ZIP, kDefaultZip}});

  // Define the length of the phone number fields to allow the parser to
  // identify them as area code, prefix and suffix.
  form_data.fields[3].max_length = 3;
  form_data.fields[4].max_length = 3;
  form_data.fields[5].max_length = 4;

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form_data);
  ImportAddressProfileAndVerifyImportOfDefaultProfile(*form_structure);
}

TEST_P(FormDataImporterTest, ImportAddressProfiles_UnFocussableFields) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultProfileFormStructure();
  // Set the Address line field as unfocusable.
  form_structure->field(4)->is_focusable = false;

  base::test::ScopedFeatureList unfocusable_feature;

  // With the feature disabled, there should be no import.
  unfocusable_feature.InitAndDisableFeature(
      features::kAutofillProfileImportFromUnfocusableFields);
  ImportAddressProfileAndVerifyImportOfNoProfile(*form_structure);

  unfocusable_feature.Reset();
  // In contrast, with the feature enabled, there should be import.
  unfocusable_feature.InitAndEnableFeature(
      features::kAutofillProfileImportFromUnfocusableFields);
  ImportAddressProfileAndVerifyImportOfDefaultProfile(*form_structure);
}

TEST_P(FormDataImporterTest, ImportAddressProfiles_MultilineAddress) {
  std::vector<std::pair<ServerFieldType, std::string>> type_value_pairs({
      {NAME_FULL, kDefaultFullName},
      // This is a multi-line field.
      {ADDRESS_HOME_STREET_ADDRESS, kDefaultStreetAddress},
      {ADDRESS_HOME_CITY, kDefaultCity},
      {ADDRESS_HOME_STATE, kDefaultState},
      {ADDRESS_HOME_ZIP, kDefaultZip},
      {ADDRESS_HOME_COUNTRY, "US"},
  });

  AutofillProfile profile =
      ConstructProfileFromTypeValuePairs(type_value_pairs);

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(type_value_pairs);
  ImportAddressProfilesAndVerifyExpectation(*form_structure, {profile});
}

TEST_P(FormDataImporterTest,
       ImportAddressProfiles_TwoValidProfilesDifferentForms) {
  std::unique_ptr<FormStructure> default_form_structure =
      ConstructDefaultProfileFormStructure();

  AutofillProfile default_profile = ConstructDefaultProfile();
  ImportAddressProfilesAndVerifyExpectation(*default_form_structure,
                                            {default_profile});

  // Now import a second profile from a different form submission.
  std::unique_ptr<FormStructure> alternative_form_structure =
      ConstructSecondProfileFormStructure();
  AutofillProfile alternative_profile = ConstructSecondProfile();

  // Verify that both profiles have been imported.
  ImportAddressProfilesAndVerifyExpectation(
      *alternative_form_structure, {alternative_profile, default_profile});
}

TEST_P(FormDataImporterTest, ImportAddressProfiles_TwoValidProfilesSameForm) {
  AutofillProfile default_profile = ConstructDefaultProfile();
  AutofillProfile alternative_profile = ConstructSecondProfile();

  // Get the type value pairs that correspond to the first profile.
  std::vector<std::pair<ServerFieldType, std::string>>
      profile_type_value_pairs = GetDefaultProfileTypeValuePairs();
  std::vector<std::pair<ServerFieldType, std::string>>
      second_profile_type_value_pairs = GetDefaultProfileTypeValuePairs();

  // Now combine the two vectors and construct the single FormStructure that
  // holds both profiles.
  profile_type_value_pairs.insert(profile_type_value_pairs.end(),
                                  second_profile_type_value_pairs.begin(),
                                  second_profile_type_value_pairs.end());
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(profile_type_value_pairs);

  ImportAddressProfilesAndVerifyExpectation(
      *form_structure, {default_profile, alternative_profile});
}

TEST_P(FormDataImporterTest,
       ImportAddressProfiles_OneValidProfileSameForm_PartsHidden) {
  FormData form_data = ConstructDefaultFormData();

  FormData hidden_second_form = form_data;
  for (FormFieldData& field : hidden_second_form.fields) {
    // Reset the values and make the field non focusable.
    field.value = u"";
    field.is_focusable = false;
  }

  // Append the fields of the second form to the first form.
  form_data.fields.insert(form_data.fields.end(),
                          hidden_second_form.fields.begin(),
                          hidden_second_form.fields.end());

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form_data);
  ImportAddressProfileAndVerifyImportOfDefaultProfile(*form_structure);
}

// A maximum of two address profiles are imported per form.
TEST_P(FormDataImporterTest, ImportAddressProfiles_ThreeValidProfilesSameForm) {
  std::vector<std::pair<ServerFieldType, std::string>>
      profile_type_value_pairs = GetDefaultProfileTypeValuePairs();

  std::vector<std::pair<ServerFieldType, std::string>>
      second_profile_type_value_pairs = GetDefaultProfileTypeValuePairs();

  std::vector<std::pair<ServerFieldType, std::string>>
      third_profile_type_value_pairs = GetDefaultProfileTypeValuePairs();

  // Merge the type value pairs into one and construct the corresponding form
  // structure.
  profile_type_value_pairs.insert(profile_type_value_pairs.end(),
                                  second_profile_type_value_pairs.begin(),
                                  second_profile_type_value_pairs.end());
  profile_type_value_pairs.insert(profile_type_value_pairs.end(),
                                  third_profile_type_value_pairs.begin(),
                                  third_profile_type_value_pairs.end());
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(profile_type_value_pairs);

  // Import from the form structure and verify that only the first two profiles
  // are imported.
  ImportAddressProfilesAndVerifyExpectation(
      *form_structure, {ConstructDefaultProfile(), ConstructSecondProfile()});
}

TEST_P(FormDataImporterTest, ImportAddressProfiles_SameProfileWithConflict) {
  std::vector<std::pair<ServerFieldType, std::string>> initial_type_value_pairs(
      {
          {NAME_FULL, kDefaultFullName},
          {ADDRESS_HOME_LINE1, kDefaultAddressLine1},
          {ADDRESS_HOME_CITY, kDefaultCity},
          {ADDRESS_HOME_STATE, kDefaultState},
          {ADDRESS_HOME_ZIP, kDefaultZip},
          {PHONE_HOME_WHOLE_NUMBER, kDefaultPhone},
      });
  AutofillProfile initial_profile =
      ConstructProfileFromTypeValuePairs(initial_type_value_pairs);

  std::unique_ptr<FormStructure> initial_form_structure =
      ConstructFormStructureFromTypeValuePairs(initial_type_value_pairs);
  ImportAddressProfilesAndVerifyExpectation(*initial_form_structure,
                                            {initial_profile});

  // Create a second form structure with an additional country and a differently
  // formatted phone number
  std::vector<std::pair<ServerFieldType, std::string>>
      conflicting_type_value_pairs(
          {{NAME_FULL, kDefaultFullName},
           {ADDRESS_HOME_LINE1, kDefaultAddressLine1},
           {ADDRESS_HOME_CITY, kDefaultCity},
           {ADDRESS_HOME_STATE, kDefaultState},
           {ADDRESS_HOME_ZIP, kDefaultZip},
           // The phone number is spelled differently.
           {PHONE_HOME_WHOLE_NUMBER, kDefaultPhoneAlternativeFormatting},
           // Country information is added.
           {ADDRESS_HOME_COUNTRY, "US"}});
  AutofillProfile conflicting_profile =
      ConstructProfileFromTypeValuePairs(conflicting_type_value_pairs);

  // Verify that the initial profile and the conflicting profile are not the
  // same.
  ASSERT_FALSE(initial_profile.Compare(conflicting_profile) == 0);

  std::unique_ptr<FormStructure> conflicting_form_structure =
      ConstructFormStructureFromTypeValuePairs(conflicting_type_value_pairs);
  // Verify that importing the conflicting profile will result in an update of
  // the existing profile rather than creating a new one.
  ImportAddressProfilesAndVerifyExpectation(*conflicting_form_structure,
                                            {conflicting_profile});
}

TEST_P(FormDataImporterTest, ImportAddressProfiles_MissingInfoInOld) {
  std::vector<std::pair<ServerFieldType, std::string>> initial_type_value_pairs(
      {
          {NAME_FULL, kDefaultFullName},
          {ADDRESS_HOME_LINE1, kDefaultAddressLine1},
          {ADDRESS_HOME_CITY, kDefaultCity},
          {ADDRESS_HOME_STATE, kDefaultState},
          {ADDRESS_HOME_ZIP, kDefaultZip},
          {PHONE_HOME_WHOLE_NUMBER, kDefaultPhone},
      });
  AutofillProfile initial_profile =
      ConstructProfileFromTypeValuePairs(initial_type_value_pairs);

  std::unique_ptr<FormStructure> initial_form_structure =
      ConstructFormStructureFromTypeValuePairs(initial_type_value_pairs);
  ImportAddressProfilesAndVerifyExpectation(*initial_form_structure,
                                            {initial_profile});

  // Create a superset that includes a new email address.
  std::vector<std::pair<ServerFieldType, std::string>>
      superset_type_value_pairs = initial_type_value_pairs;
  superset_type_value_pairs.emplace_back(EMAIL_ADDRESS, kDefaultMail);

  AutofillProfile superset_profile =
      ConstructProfileFromTypeValuePairs(superset_type_value_pairs);

  // Verify that the initial profile and the superset profile are not the
  // same.
  ASSERT_FALSE(initial_profile.Compare(superset_profile) == 0);

  std::unique_ptr<FormStructure> superset_form_structure =
      ConstructFormStructureFromTypeValuePairs(superset_type_value_pairs);
  // Verify that importing the superset profile will result in an update of
  // the existing profile rather than creating a new one.
  ImportAddressProfilesAndVerifyExpectation(*superset_form_structure,
                                            {superset_profile});
}

TEST_P(FormDataImporterTest, ImportAddressProfiles_MissingInfoInNew) {
  std::vector<std::pair<ServerFieldType, std::string>> subset_type_value_pairs({
      {NAME_FULL, kDefaultFullName},
      {ADDRESS_HOME_LINE1, kDefaultAddressLine1},
      {ADDRESS_HOME_CITY, kDefaultCity},
      {ADDRESS_HOME_STATE, kDefaultState},
      {ADDRESS_HOME_ZIP, kDefaultZip},
      {PHONE_HOME_WHOLE_NUMBER, kDefaultPhone},
  });
  // Create a superset that includes a new email address.
  std::vector<std::pair<ServerFieldType, std::string>>
      superset_type_value_pairs = subset_type_value_pairs;
  superset_type_value_pairs.emplace_back(EMAIL_ADDRESS, kDefaultMail);

  AutofillProfile subset_profile =
      ConstructProfileFromTypeValuePairs(subset_type_value_pairs);
  AutofillProfile superset_profile =
      ConstructProfileFromTypeValuePairs(superset_type_value_pairs);

  // Verify that the subset profile and the superset profile are not the
  // same.
  ASSERT_FALSE(subset_profile.Compare(superset_profile) == 0);

  // First import the superset profile.
  std::unique_ptr<FormStructure> superset_form_structure =
      ConstructFormStructureFromTypeValuePairs(superset_type_value_pairs);
  ImportAddressProfilesAndVerifyExpectation(*superset_form_structure,
                                            {superset_profile});

  // Than import the subset profile and verify that the stored profile is still
  // the superset.
  std::unique_ptr<FormStructure> subset_form_structure =
      ConstructFormStructureFromTypeValuePairs(subset_type_value_pairs);
  ImportAddressProfilesAndVerifyExpectation(*superset_form_structure,
                                            {superset_profile});
}

TEST_P(FormDataImporterTest, ImportAddressProfiles_InsufficientAddress) {
  // This address is missing a state which is required in the US.
  std::vector<std::pair<ServerFieldType, std::string>> type_value_pairs({
      {NAME_FULL, kDefaultFullName},
      {ADDRESS_HOME_LINE1, kDefaultAddressLine1},
      {ADDRESS_HOME_CITY, kDefaultCity},
      {ADDRESS_HOME_ZIP, kDefaultZip},
      {ADDRESS_HOME_COUNTRY, "US"},
  });

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(type_value_pairs);
  // Verify that no profile is imported.
  ImportAddressProfileAndVerifyImportOfNoProfile(*form_structure);
}

// Ensure that if a verified profile already exists, aggregated profiles cannot
// modify it in any way. This also checks the profile merging/matching algorithm
// works: if either the full name OR all the non-empty name pieces match, the
// profile is a match.
TEST_P(FormDataImporterTest,
       ImportAddressProfiles_ExistingVerifiedProfileWithConflict) {
  // Start with a verified profile.
  AutofillProfile profile(base::GenerateGUID(), kSettingsOrigin);
  test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  EXPECT_TRUE(profile.IsVerified());

  base::RunLoop run_loop;
  EXPECT_CALL(personal_data_observer_, OnPersonalDataFinishedProfileTasks())
      .WillOnce(QuitMessageLoop(&run_loop));
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged()).Times(1);
  personal_data_manager_->AddProfile(profile);
  run_loop.Run();

  // Simulate a form submission with conflicting info.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("First name:", "first_name", "Marion Mitchell",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Morrison", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "johnwayne@me.xyz", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "123 Zoo St.", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "Hollywood", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "CA", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "91601", "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  ImportAddressProfiles(/*extraction_successful=*/true, form_structure);

  // Expect that no new profile is saved.
  const std::vector<AutofillProfile*>& results =
      personal_data_manager_->GetProfiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, profile.Compare(*results[0]));

  // Try the same thing, but without "Mitchell". The profiles should still match
  // because the non empty name pieces (first and last) match that stored in the
  // profile.
  test::CreateTestFormField("First name:", "first_name", "Marion", "text",
                            &field);
  form.fields[0] = field;

  FormStructure form_structure2(form);
  form_structure2.DetermineHeuristicTypes(nullptr, nullptr);

  ImportAddressProfiles(/*extraction_successful=*/true, form_structure2);

  // Expect that no new profile is saved.
  const std::vector<AutofillProfile*>& results2 =
      personal_data_manager_->GetProfiles();
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, profile.Compare(*results2[0]));
}

TEST_P(FormDataImporterTest,
       IncorporateStructuredNameInformationInVerifiedProfile) {
  // This test is only applicable to structured names.
  if (!structured_address::StructuredNamesEnabled()) {
    return;
  }

  // Start with a verified profile.
  AutofillProfile profile(base::GenerateGUID(), kSettingsOrigin);
  test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  EXPECT_TRUE(profile.IsVerified());

  // Set the verification status for the first and middle name to parsed.
  profile.SetRawInfoWithVerificationStatus(
      NAME_FIRST, u"Marion", structured_address::VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(
      NAME_MIDDLE, u"Mitchell",
      structured_address::VerificationStatus::kParsed);

  base::RunLoop run_loop;
  EXPECT_CALL(personal_data_observer_, OnPersonalDataFinishedProfileTasks())
      .WillOnce(QuitMessageLoop(&run_loop));
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged()).Times(1);
  personal_data_manager_->AddProfile(profile);
  run_loop.Run();

  // Simulate a form submission with conflicting info.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("First name:", "first_name", "Marion Mitchell",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Morrison", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "johnwayne@me.xyz", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "123 Zoo St.", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "Hollywood", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "CA", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "91601", "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  ImportAddressProfiles(/*extraction_successful=*/true, form_structure);

  // The form submission should result in a change of name structure.
  profile.SetRawInfoWithVerificationStatus(
      NAME_FIRST, u"Marion Mitchell",
      structured_address::VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(
      NAME_MIDDLE, u"", structured_address::VerificationStatus::kNoStatus);
  profile.SetRawInfoWithVerificationStatus(
      NAME_LAST, u"Morrison",
      structured_address::VerificationStatus::kObserved);

  // Expect that no new profile is saved.
  const std::vector<AutofillProfile*>& results =
      personal_data_manager_->GetProfiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, profile.Compare(*results[0]));

  // Try the same thing, but without "Mitchell". The profiles should still match
  // because "Marion Morrison" is a variant of the known full name.
  test::CreateTestFormField("First name:", "first_name", "Marion", "text",
                            &field);
  form.fields[0] = field;

  FormStructure form_structure2(form);
  form_structure2.DetermineHeuristicTypes(nullptr, nullptr);

  ImportAddressProfiles(/*extraction_successful=*/true, form_structure2);

  // Expect that no new profile is saved.
  const std::vector<AutofillProfile*>& results2 =
      personal_data_manager_->GetProfiles();
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, profile.Compare(*results2[0]));
}

TEST_P(FormDataImporterTest,
       IncorporateStructuredAddressInformationInVerififedProfile) {
  // This test is only applicable to structured addresses.
  if (!structured_address::StructuredAddressesEnabled()) {
    return;
  }

  // Start with a verified profile.
  AutofillProfile profile(base::GenerateGUID(), kSettingsOrigin);
  test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  EXPECT_TRUE(profile.IsVerified());

  // Reset the structured address to emulate a failed parsing attempt.
  profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_HOUSE_NUMBER, u"",
      structured_address::VerificationStatus::kNoStatus);
  profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_NAME, u"",
      structured_address::VerificationStatus::kNoStatus);
  profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_AND_DEPENDENT_STREET_NAME, u"",
      structured_address::VerificationStatus::kNoStatus);

  base::RunLoop run_loop;
  EXPECT_CALL(personal_data_observer_, OnPersonalDataFinishedProfileTasks())
      .WillOnce(QuitMessageLoop(&run_loop));
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged()).Times(1);
  personal_data_manager_->AddProfile(profile);
  run_loop.Run();

  // Simulate a form submission with conflicting info.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("First name:", "first_name", "Marion Mitchell",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Morrison", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "johnwayne@me.xyz", "text",
                            &field);
  form.fields.push_back(field);
  // This forms contains structured address information.
  test::CreateTestFormField("Street Name:", "street_name", "Zoo St.", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("House Number:", "house_number", "123", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "Hollywood", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "CA", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "91601", "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  ImportAddressProfiles(/*extraction_successful=*/true, form_structure);

  // The form submission should result in a change of the address structure.
  profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_AND_DEPENDENT_STREET_NAME, u"Zoo St.",
      structured_address::VerificationStatus::kFormatted);
  profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_NAME, u"Zoo St.",
      structured_address::VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_HOUSE_NUMBER, u"123",
      structured_address::VerificationStatus::kObserved);

  // Expect that no new profile is saved.
  const std::vector<AutofillProfile*>& results =
      personal_data_manager_->GetProfiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, profile.Compare(*results[0]));
}

// Tests that no profile is inferred if the country is not recognized.
TEST_P(FormDataImporterTest, ImportAddressProfiles_UnrecognizedCountry) {
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("First name:", "first_name", "George", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Washington", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "theprez@gmail.com", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "21 Laussat St", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Country:", "country", "Notacountry", "text",
                            &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  ImportAddressProfiles(/*extraction_successful=*/false, form_structure);

  // Since no refresh is expected, reload the data from the database to make
  // sure no changes were written out.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  ASSERT_EQ(0U, personal_data_manager_->GetProfiles().size());
  ASSERT_EQ(0U, personal_data_manager_->GetCreditCards().size());
}

// Tests that a profile is imported if the country can be translated using the
// page language.
TEST_P(FormDataImporterTest, ImportAddressProfiles_LocalizedCountryName) {
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  // Create a form with all important fields.
  FormFieldData field;
  test::CreateTestFormField("First name:", "first_name", "George", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Washington", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "theprez@gmail.com", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "21 Laussat St", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);
  // The country field has a localized value.
  test::CreateTestFormField("Country:", "country", "Armenien", "text", &field);
  form.fields.push_back(field);

  // Set up language state mock.
  autofill_client_->GetLanguageState()->SetSourceLanguage("");

  // Verify that the country code is not determined from the country value if
  // the page language is not set.
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  ImportAddressProfiles(/*extraction_successful=*/false, form_structure);

  ASSERT_EQ(0U, personal_data_manager_->GetProfiles().size());
  ASSERT_EQ(0U, personal_data_manager_->GetCreditCards().size());

  // Set the page language to match the localized country value and try again.
  autofill_client_->GetLanguageState()->SetSourceLanguage("de");

  ImportAddressProfiles(/*extraction_successful=*/true, form_structure);

  // There should be one imported address profile.
  ASSERT_EQ(1U, personal_data_manager_->GetProfiles().size());
  ASSERT_EQ(0U, personal_data_manager_->GetCreditCards().size());

  // Check that the correct profile was stored.
  AutofillProfile expected(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&expected, "George", nullptr, "Washington",
                       "theprez@gmail.com", nullptr, "21 Laussat St", nullptr,
                       "San Francisco", "California", "94102", "AM", nullptr);
  const std::vector<AutofillProfile*>& results =
      personal_data_manager_->GetProfiles();
  EXPECT_EQ(0, expected.Compare(*results[0]));
}

// Tests that a profile is created for countries with composed names.
TEST_P(FormDataImporterTest,
       ImportAddressProfiles_CompleteComposedCountryName) {
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("First name:", "first_name", "George", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Washington", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "theprez@gmail.com", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "21 Laussat St", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Country:", "country", "Myanmar [Burma]", "text",
                            &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  ImportAddressProfiles(/*extraction_successful=*/true, form_structure);

  AutofillProfile expected(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&expected, "George", nullptr, "Washington",
                       "theprez@gmail.com", nullptr, "21 Laussat St", nullptr,
                       "San Francisco", "California", "94102", "MM", nullptr);
  const std::vector<AutofillProfile*>& results =
      personal_data_manager_->GetProfiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected.Compare(*results[0]));
}

// TODO(crbug.com/634131): Create profiles if part of a standalone part of a
// composed country name is present.
// Tests that a profile is created if a standalone part of a composed country
// name is present.
TEST_P(FormDataImporterTest,
       ImportAddressProfiles_IncompleteComposedCountryName) {
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("First name:", "first_name", "George", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Washington", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "theprez@gmail.com", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "21 Laussat St", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Country:", "country",
                            "Myanmar",  // Missing the [Burma] part
                            "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  ImportAddressProfiles(/*extraction_successful=*/false, form_structure);

  // Since no refresh is expected, reload the data from the database to make
  // sure no changes were written out.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  ASSERT_EQ(0U, personal_data_manager_->GetProfiles().size());
  ASSERT_EQ(0U, personal_data_manager_->GetCreditCards().size());
}

// ImportCreditCard tests.

// Tests that a valid credit card is extracted.
TEST_P(FormDataImporterTest, ImportCreditCard_Valid) {
  // Add a single valid credit card form.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form, "Biggie Smalls", "4111-1111-1111-1111", "01",
                        "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  std::unique_ptr<CreditCard> imported_credit_card;
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(ImportCreditCard(form_structure, false, &imported_credit_card));
  ASSERT_TRUE(imported_credit_card);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SubmittedCardState",
      AutofillMetrics::HAS_CARD_NUMBER_AND_EXPIRATION_DATE, 1);
  personal_data_manager_->OnAcceptedLocalCreditCardSave(*imported_credit_card);

  WaitForOnPersonalDataChanged();

  CreditCard expected(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected, "Biggie Smalls", "4111111111111111", "01",
                          "2999", "");  // Imported cards have no billing info.
  const std::vector<CreditCard*>& results =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected.Compare(*results[0]));
}

// Tests that an invalid credit card number is not extracted.
TEST_P(FormDataImporterTest, ImportCreditCard_InvalidCardNumber) {
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form, "Jim Johansen", "1000000000000000", "02",
                        "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  std::unique_ptr<CreditCard> imported_credit_card;
  base::HistogramTester histogram_tester;
  EXPECT_FALSE(ImportCreditCard(form_structure, false, &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);
  histogram_tester.ExpectUniqueSample("Autofill.SubmittedCardState",
                                      AutofillMetrics::HAS_EXPIRATION_DATE_ONLY,
                                      1);

  // Since no refresh is expected, reload the data from the database to make
  // sure no changes were written out.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  ASSERT_EQ(0U, personal_data_manager_->GetCreditCards().size());
}

// Tests that a credit card with an empty expiration can be extracted due to the
// expiration date fix flow.
TEST_P(FormDataImporterTest,
       ImportCreditCard_InvalidExpiryDate_EditableExpirationExpOn) {
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form, "Smalls Biggie", "4111-1111-1111-1111", "", "");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  std::unique_ptr<CreditCard> imported_credit_card;
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(ImportCreditCard(form_structure, false, &imported_credit_card));
  ASSERT_TRUE(imported_credit_card);
  histogram_tester.ExpectUniqueSample("Autofill.SubmittedCardState",
                                      AutofillMetrics::HAS_CARD_NUMBER_ONLY, 1);
}

// Tests that an expired credit card can be extracted due to the expiration date
// fix flow.
TEST_P(FormDataImporterTest,
       ImportCreditCard_ExpiredExpiryDate_EditableExpirationExpOn) {
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form, "Smalls Biggie", "4111-1111-1111-1111", "01",
                        "2000");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  std::unique_ptr<CreditCard> imported_credit_card;
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(ImportCreditCard(form_structure, false, &imported_credit_card));
  ASSERT_TRUE(imported_credit_card);
  histogram_tester.ExpectUniqueSample("Autofill.SubmittedCardState",
                                      AutofillMetrics::HAS_CARD_NUMBER_ONLY, 1);
}

// Tests that a valid credit card is extracted when the option text for month
// select can't be parsed but its value can.
TEST_P(FormDataImporterTest, ImportCreditCard_MonthSelectInvalidText) {
  // Add a single valid credit card form with an invalid option value.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form, "Biggie Smalls", "4111-1111-1111-1111",
                        "Feb (2)", "2999");
  // Add option values and contents to the expiration month field.
  ASSERT_EQ(u"exp_month", form.fields[2].name);
  form.fields[2].options = {
      {.value = u"1", .content = u"Jan (1)"},
      {.value = u"2", .content = u"Feb (2)"},
      {.value = u"3", .content = u"Mar (3)"},
  };

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  std::unique_ptr<CreditCard> imported_credit_card;
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(ImportCreditCard(form_structure, false, &imported_credit_card));
  ASSERT_TRUE(imported_credit_card);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SubmittedCardState",
      AutofillMetrics::HAS_CARD_NUMBER_AND_EXPIRATION_DATE, 1);
  personal_data_manager_->OnAcceptedLocalCreditCardSave(*imported_credit_card);

  WaitForOnPersonalDataChanged();

  // See that the invalid option text was converted to the right value.
  CreditCard expected(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected, "Biggie Smalls", "4111111111111111", "02",
                          "2999", "");  // Imported cards have no billing info.
  const std::vector<CreditCard*>& results =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected.Compare(*results[0]));
}

TEST_P(FormDataImporterTest, ImportCreditCard_TwoValidCards) {
  // Start with a single valid credit card form.
  FormData form1;
  form1.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form1, "Biggie Smalls", "4111-1111-1111-1111", "01",
                        "2999");

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes(nullptr, nullptr);
  std::unique_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(ImportCreditCard(form_structure1, false, &imported_credit_card));
  ASSERT_TRUE(imported_credit_card);
  personal_data_manager_->OnAcceptedLocalCreditCardSave(*imported_credit_card);

  WaitForOnPersonalDataChanged();

  CreditCard expected(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected, "Biggie Smalls", "4111111111111111", "01",
                          "2999", "");  // Imported cards have no billing info.
  const std::vector<CreditCard*>& results =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected.Compare(*results[0]));

  // Add a second different valid credit card.
  FormData form2;
  form2.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form2, "", "5500 0000 0000 0004", "02", "2999");

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes(nullptr, nullptr);

  std::unique_ptr<CreditCard> imported_credit_card2;
  EXPECT_TRUE(ImportCreditCard(form_structure2, false, &imported_credit_card2));
  ASSERT_TRUE(imported_credit_card2);
  personal_data_manager_->OnAcceptedLocalCreditCardSave(*imported_credit_card2);

  WaitForOnPersonalDataChanged();

  CreditCard expected2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected2, "", "5500000000000004", "02", "2999",
                          "");  // Imported cards have no billing info.
  std::vector<CreditCard*> cards;
  cards.push_back(&expected);
  cards.push_back(&expected2);
  ExpectSameElements(cards, personal_data_manager_->GetCreditCards());
}

// This form has the expiration year as one field with MM/YY.
TEST_P(FormDataImporterTest, ImportCreditCard_Month2DigitYearCombination) {
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name on card:", "name_on_card", "John MMYY",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number:", "card_number", "4111111111111111",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Date:", "exp_date", "05/45", "text", &field);
  field.autocomplete_attribute = "cc-exp";
  field.max_length = 5;
  form.fields.push_back(field);

  SubmitFormAndExpectImportedCardWithData(form, "John MMYY", "4111111111111111",
                                          "05", "2045");
}

// This form has the expiration year as one field with MM/YYYY.
TEST_P(FormDataImporterTest, ImportCreditCard_Month4DigitYearCombination) {
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name on card:", "name_on_card", "John MMYYYY",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number:", "card_number", "4111111111111111",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Date:", "exp_date", "05/2045", "text", &field);
  field.autocomplete_attribute = "cc-exp";
  field.max_length = 7;
  form.fields.push_back(field);

  SubmitFormAndExpectImportedCardWithData(form, "John MMYYYY",
                                          "4111111111111111", "05", "2045");
}

// This form has the expiration year as one field with M/YYYY.
TEST_P(FormDataImporterTest, ImportCreditCard_1DigitMonth4DigitYear) {
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name on card:", "name_on_card", "John MYYYY",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number:", "card_number", "4111111111111111",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Date:", "exp_date", "5/2045", "text", &field);
  field.autocomplete_attribute = "cc-exp";
  form.fields.push_back(field);

  SubmitFormAndExpectImportedCardWithData(form, "John MYYYY",
                                          "4111111111111111", "05", "2045");
}

// This form has the expiration year as a 2-digit field.
TEST_P(FormDataImporterTest, ImportCreditCard_2DigitYear) {
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name on card:", "name_on_card", "John Smith",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number:", "card_number", "4111111111111111",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "05", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "45", "text", &field);
  field.max_length = 2;
  form.fields.push_back(field);

  SubmitFormAndExpectImportedCardWithData(form, "John Smith",
                                          "4111111111111111", "05", "2045");
}

// Tests that a credit card is not extracted because the
// card matches a masked server card.
TEST_P(FormDataImporterTest,
       ImportCreditCard_DuplicateServerCards_MaskedCard_DontExtract) {
  // Add a masked server card.
  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::MASKED_SERVER_CARD, "a123"));
  test::SetCreditCardInfo(&server_cards.back(), "John Dillinger",
                          "1111" /* Visa */, "01", "2999", "");
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);
  test::SetServerCreditCards(autofill_table_, server_cards);

  // Make sure everything is set up correctly.
  personal_data_manager_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_manager_->GetCreditCards().size());

  // Type the same data as the masked card into a form.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form, "John Dillinger", "4111111111111111", "01",
                        "2999");

  // The card should not be offered to be saved locally because the feature flag
  // is disabled.
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  std::unique_ptr<CreditCard> imported_credit_card;
  EXPECT_FALSE(ImportCreditCard(form_structure, false, &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);
}

// Tests that a credit card is not extracted because it matches a full server
// card.
TEST_P(FormDataImporterTest, ImportCreditCard_DuplicateServerCards_FullCard) {
  // Add a full server card.
  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::FULL_SERVER_CARD, "c789"));
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2999", "");  // Imported cards have no billing info.
  test::SetServerCreditCards(autofill_table_, server_cards);

  // Make sure everything is set up correctly.
  personal_data_manager_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_manager_->GetCreditCards().size());

  // Type the same data as the unmasked card into a form.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form, "Clyde Barrow", "378282246310005", "04", "2999");

  // The card should not be offered to be saved locally because it only matches
  // the full server card.
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  std::unique_ptr<CreditCard> imported_credit_card;
  EXPECT_FALSE(ImportCreditCard(form_structure, false, &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);
}

TEST_P(FormDataImporterTest, ImportCreditCard_SameCreditCardWithConflict) {
  // Start with a single valid credit card form.
  FormData form1;
  form1.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form1, "Biggie Smalls", "4111-1111-1111-1111", "01",
                        "2998");

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes(nullptr, nullptr);
  std::unique_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(ImportCreditCard(form_structure1, false, &imported_credit_card));
  ASSERT_TRUE(imported_credit_card);
  personal_data_manager_->OnAcceptedLocalCreditCardSave(*imported_credit_card);

  WaitForOnPersonalDataChanged();

  CreditCard expected(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected, "Biggie Smalls", "4111111111111111", "01",
                          "2998", "");  // Imported cards have no billing info.
  const std::vector<CreditCard*>& results =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected.Compare(*results[0]));

  // Add a second different valid credit card where the year is different but
  // the credit card number matches.
  FormData form2;
  form2.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form2, "Biggie Smalls", "4111 1111 1111 1111", "01",
                        /* different year */ "2999");

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes(nullptr, nullptr);
  std::unique_ptr<CreditCard> imported_credit_card2;
  EXPECT_TRUE(ImportCreditCard(form_structure2, false, &imported_credit_card2));
  EXPECT_FALSE(imported_credit_card2);

  WaitForOnPersonalDataChanged();

  // Expect that the newer information is saved.  In this case the year is
  // updated to "2999".
  CreditCard expected2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected2, "Biggie Smalls", "4111111111111111", "01",
                          "2999", "");  // Imported cards have no billing info.
  const std::vector<CreditCard*>& results2 =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, expected2.Compare(*results2[0]));
}

TEST_P(FormDataImporterTest, ImportCreditCard_ShouldReturnLocalCard) {
  // Start with a single valid credit card form.
  FormData form1;
  form1.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form1, "Biggie Smalls", "4111-1111-1111-1111", "01",
                        "2998");

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes(nullptr, nullptr);
  std::unique_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(ImportCreditCard(form_structure1, false, &imported_credit_card));
  ASSERT_TRUE(imported_credit_card);
  personal_data_manager_->OnAcceptedLocalCreditCardSave(*imported_credit_card);

  WaitForOnPersonalDataChanged();

  CreditCard expected(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected, "Biggie Smalls", "4111111111111111", "01",
                          "2998", "");  // Imported cards have no billing info.
  const std::vector<CreditCard*>& results =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected.Compare(*results[0]));

  // Add a second different valid credit card where the year is different but
  // the credit card number matches.
  FormData form2;
  form2.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form2, "Biggie Smalls", "4111 1111 1111 1111", "01",
                        /* different year */ "2999");

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes(nullptr, nullptr);
  std::unique_ptr<CreditCard> imported_credit_card2;
  EXPECT_TRUE(ImportCreditCard(form_structure2,
                               /* should_return_local_card= */ true,
                               &imported_credit_card2));
  // The local card is returned after an update.
  EXPECT_TRUE(imported_credit_card2);

  WaitForOnPersonalDataChanged();

  // Expect that the newer information is saved.  In this case the year is
  // updated to "2999".
  CreditCard expected2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected2, "Biggie Smalls", "4111111111111111", "01",
                          "2999", "");  // Imported cards have no billing info.
  const std::vector<CreditCard*>& results2 =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, expected2.Compare(*results2[0]));
}

TEST_P(FormDataImporterTest, ImportCreditCard_EmptyCardWithConflict) {
  // Start with a single valid credit card form.
  FormData form1;
  form1.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form1, "Biggie Smalls", "4111-1111-1111-1111", "01",
                        "2998");

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes(nullptr, nullptr);

  std::unique_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(ImportCreditCard(form_structure1, false, &imported_credit_card));
  ASSERT_TRUE(imported_credit_card);
  personal_data_manager_->OnAcceptedLocalCreditCardSave(*imported_credit_card);

  WaitForOnPersonalDataChanged();

  CreditCard expected(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected, "Biggie Smalls", "4111111111111111", "01",
                          "2998", "");  // Imported cards have no billing info.
  const std::vector<CreditCard*>& results =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected.Compare(*results[0]));

  // Add a second credit card with no number.
  FormData form2;
  form2.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form2, "Biggie Smalls", /* no number */ nullptr, "01",
                        "2999");

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes(nullptr, nullptr);
  std::unique_ptr<CreditCard> imported_credit_card2;
  EXPECT_FALSE(
      ImportCreditCard(form_structure2, false, &imported_credit_card2));
  EXPECT_FALSE(imported_credit_card2);

  // Since no refresh is expected, reload the data from the database to make
  // sure no changes were written out.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // No change is expected.
  CreditCard expected2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected2, "Biggie Smalls", "4111111111111111", "01",
                          "2998", "");
  const std::vector<CreditCard*>& results2 =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, expected2.Compare(*results2[0]));
}

TEST_P(FormDataImporterTest, ImportCreditCard_MissingInfoInNew) {
  // Start with a single valid credit card form.
  FormData form1;
  form1.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form1, "Biggie Smalls", "4111-1111-1111-1111", "01",
                        "2999");

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes(nullptr, nullptr);
  std::unique_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(ImportCreditCard(form_structure1, false, &imported_credit_card));
  ASSERT_TRUE(imported_credit_card);
  personal_data_manager_->OnAcceptedLocalCreditCardSave(*imported_credit_card);

  WaitForOnPersonalDataChanged();

  CreditCard expected(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected, "Biggie Smalls", "4111111111111111", "01",
                          "2999", "");
  const std::vector<CreditCard*>& results =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected.Compare(*results[0]));

  // Add a second different valid credit card where the name is missing but
  // the credit card number matches.
  FormData form2;
  form2.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form2, /* missing name */ nullptr,
                        "4111-1111-1111-1111", "01", "2999");

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes(nullptr, nullptr);
  std::unique_ptr<CreditCard> imported_credit_card2;
  EXPECT_TRUE(ImportCreditCard(form_structure2, false, &imported_credit_card2));
  EXPECT_FALSE(imported_credit_card2);

  // Since no refresh is expected, reload the data from the database to make
  // sure no changes were written out.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // No change is expected.
  CreditCard expected2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected2, "Biggie Smalls", "4111111111111111", "01",
                          "2999", "");
  const std::vector<CreditCard*>& results2 =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, expected2.Compare(*results2[0]));

  // Add a third credit card where the expiration date is missing.
  FormData form3;
  form3.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form3, "Johnny McEnroe", "5555555555554444",
                        /* no month */ nullptr,
                        /* no year */ nullptr);

  FormStructure form_structure3(form3);
  form_structure3.DetermineHeuristicTypes(nullptr, nullptr);
  std::unique_ptr<CreditCard> imported_credit_card3;
  EXPECT_FALSE(
      ImportCreditCard(form_structure3, false, &imported_credit_card3));
  ASSERT_FALSE(imported_credit_card3);

  // Since no refresh is expected, reload the data from the database to make
  // sure no changes were written out.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // No change is expected.
  CreditCard expected3(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected3, "Biggie Smalls", "4111111111111111", "01",
                          "2999", "");
  const std::vector<CreditCard*>& results3 =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results3.size());
  EXPECT_EQ(0, expected3.Compare(*results3[0]));
}

TEST_P(FormDataImporterTest, ImportCreditCard_MissingInfoInOld) {
  // Start with a single valid credit card stored via the preferences.
  // Note the empty name.
  CreditCard saved_credit_card(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&saved_credit_card, "", "4111111111111111" /* Visa */,
                          "01", "2998", "1");
  personal_data_manager_->AddCreditCard(saved_credit_card);

  WaitForOnPersonalDataChanged();

  const std::vector<CreditCard*>& results1 =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results1.size());
  EXPECT_EQ(saved_credit_card, *results1[0]);

  // Add a second different valid credit card where the year is different but
  // the credit card number matches.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form, "Biggie Smalls", "4111-1111-1111-1111", "01",
                        /* different year */ "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  std::unique_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(ImportCreditCard(form_structure, false, &imported_credit_card));
  EXPECT_FALSE(imported_credit_card);

  WaitForOnPersonalDataChanged();

  // Expect that the newer information is saved.  In this case the year is
  // added to the existing credit card.
  CreditCard expected2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected2, "Biggie Smalls", "4111111111111111", "01",
                          "2999", "1");
  const std::vector<CreditCard*>& results2 =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, expected2.Compare(*results2[0]));
}

// We allow the user to store a credit card number with separators via the UI.
// We should not try to re-aggregate the same card with the separators stripped.
TEST_P(FormDataImporterTest, ImportCreditCard_SameCardWithSeparators) {
  // Start with a single valid credit card stored via the preferences.
  // Note the separators in the credit card number.
  CreditCard saved_credit_card(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&saved_credit_card, "Biggie Smalls",
                          "4111 1111 1111 1111" /* Visa */, "01", "2999", "");
  personal_data_manager_->AddCreditCard(saved_credit_card);

  WaitForOnPersonalDataChanged();

  const std::vector<CreditCard*>& results1 =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results1.size());
  EXPECT_EQ(0, saved_credit_card.Compare(*results1[0]));

  // Import the same card info, but with different separators in the number.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form, "Biggie Smalls", "4111-1111-1111-1111", "01",
                        "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  std::unique_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(ImportCreditCard(form_structure, false, &imported_credit_card));
  EXPECT_FALSE(imported_credit_card);

  // Since no refresh is expected, reload the data from the database to make
  // sure no changes were written out.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // Expect that no new card is saved.
  const std::vector<CreditCard*>& results2 =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, saved_credit_card.Compare(*results2[0]));
}

// Ensure that if a verified credit card already exists, aggregated credit cards
// cannot modify it in any way.
TEST_P(FormDataImporterTest,
       ImportCreditCard_ExistingVerifiedCardWithConflict) {
  // Start with a verified credit card.
  CreditCard credit_card(base::GenerateGUID(), kSettingsOrigin);
  test::SetCreditCardInfo(&credit_card, "Biggie Smalls",
                          "4111 1111 1111 1111" /* Visa */, "01", "2998", "");
  EXPECT_TRUE(credit_card.IsVerified());

  // Add the credit card to the database.
  personal_data_manager_->AddCreditCard(credit_card);

  // Make sure everything is set up correctly.
  personal_data_manager_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_manager_->GetCreditCards().size());

  // Simulate a form submission with conflicting expiration year.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form, "Biggie Smalls", "4111 1111 1111 1111", "01",
                        /* different year */ "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  std::unique_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(ImportCreditCard(form_structure, false, &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  // Since no refresh is expected, reload the data from the database to make
  // sure no changes were written out.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // Expect that the saved credit card is not modified.
  const std::vector<CreditCard*>& results =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, credit_card.Compare(*results[0]));
}

// Ensures that |imported_credit_card_record_type_| is set and reset correctly.
TEST_P(FormDataImporterTest,
       ImportFormData_SecondImportResetsCreditCardRecordType) {
  // Start with a single valid credit card stored via the preferences.
  CreditCard saved_credit_card(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&saved_credit_card, "Biggie Smalls",
                          "4111 1111 1111 1111" /* Visa */, "01", "2999", "");
  personal_data_manager_->AddCreditCard(saved_credit_card);

  WaitForOnPersonalDataChanged();

  const std::vector<CreditCard*>& results =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, saved_credit_card.Compare(*results[0]));

  // Simulate a form submission with the same card.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form, "Biggie Smalls", "4111 1111 1111 1111", "01",
                        "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  std::unique_ptr<CreditCard> imported_credit_card;
  absl::optional<std::string> imported_upi_id;
  EXPECT_TRUE(ImportFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/true, &imported_credit_card,
      &imported_upi_id));
  ASSERT_TRUE(imported_credit_card);
  // |imported_credit_card_record_type_| should be LOCAL_CARD because upload was
  // offered and the card is a local card already on the device.
  ASSERT_TRUE(form_data_importer_->imported_credit_card_record_type_ ==
              FormDataImporter::ImportedCreditCardRecordType::LOCAL_CARD);

  // Second form is filled with a new card so
  // |imported_credit_card_record_type_| should be reset.
  // Simulate a form submission with a new card.
  FormData form2;
  form2.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form2, "Biggie Smalls", "4012888888881881", "01",
                        "2999");

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes(nullptr, nullptr);
  std::unique_ptr<CreditCard> imported_credit_card2;
  EXPECT_TRUE(ImportFormDataAndProcessAddressCandidates(
      form_structure2, /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/true, &imported_credit_card2,
      &imported_upi_id));
  ASSERT_TRUE(imported_credit_card2);
  // |imported_credit_card_record_type_| should be NEW_CARD because the imported
  // card is not already on the device.
  ASSERT_TRUE(form_data_importer_->imported_credit_card_record_type_ ==
              FormDataImporter::ImportedCreditCardRecordType::NEW_CARD);

  // Third form is an address form and set |credit_card_autofill_enabled| to be
  // false so that the ImportCreditCard won't be called.
  // |imported_credit_card_record_type_| should still be reset even if
  // ImportCreditCard is not called. Simulate a form submission with no card.
  FormData form3;
  form3.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("First name:", "first_name", "George", "text",
                            &field);
  form3.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Washington", "text",
                            &field);
  form3.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "bogus@example.com", "text",
                            &field);
  form3.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "21 Laussat St", "text",
                            &field);
  form3.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form3.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form3.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form3.fields.push_back(field);
  FormStructure form_structure3(form3);
  form_structure3.DetermineHeuristicTypes(nullptr, nullptr);
  std::unique_ptr<CreditCard> imported_credit_card3;
  EXPECT_TRUE(ImportFormDataAndProcessAddressCandidates(
      form_structure3, /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/false,
      /*should_return_local_card=*/true, &imported_credit_card3,
      &imported_upi_id));
  // |imported_credit_card_record_type_| should be NO_CARD because no valid card
  // was imported from the form.
  ASSERT_TRUE(form_data_importer_->imported_credit_card_record_type_ ==
              FormDataImporter::ImportedCreditCardRecordType::NO_CARD);
}

// Ensures that |imported_credit_card_record_type_| is set correctly.
TEST_P(FormDataImporterTest,
       ImportFormData_ImportCreditCardRecordType_NewCard) {
  // Simulate a form submission with a new credit card.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form, "Biggie Smalls", "4111 1111 1111 1111", "01",
                        "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  std::unique_ptr<CreditCard> imported_credit_card;
  absl::optional<std::string> imported_upi_id;
  EXPECT_TRUE(ImportFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/true, &imported_credit_card,
      &imported_upi_id));
  ASSERT_TRUE(imported_credit_card);
  // |imported_credit_card_record_type_| should be NEW_CARD because the imported
  // card is not already on the device.
  ASSERT_TRUE(form_data_importer_->imported_credit_card_record_type_ ==
              FormDataImporter::ImportedCreditCardRecordType::NEW_CARD);
}

// Ensures that |imported_credit_card_record_type_| is set correctly.
TEST_P(FormDataImporterTest,
       ImportFormData_ImportCreditCardRecordType_LocalCard) {
  // Start with a single valid credit card stored via the preferences.
  CreditCard saved_credit_card(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&saved_credit_card, "Biggie Smalls",
                          "4111 1111 1111 1111" /* Visa */, "01", "2999", "");
  personal_data_manager_->AddCreditCard(saved_credit_card);

  WaitForOnPersonalDataChanged();

  const std::vector<CreditCard*>& results =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, saved_credit_card.Compare(*results[0]));

  // Simulate a form submission with the same card.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form, "Biggie Smalls", "4111 1111 1111 1111", "01",
                        "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  std::unique_ptr<CreditCard> imported_credit_card;
  absl::optional<std::string> imported_upi_id;
  EXPECT_TRUE(ImportFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/true, &imported_credit_card,
      &imported_upi_id));
  ASSERT_TRUE(imported_credit_card);
  // |imported_credit_card_record_type_| should be LOCAL_CARD because upload was
  // offered and the card is a local card already on the device.
  ASSERT_TRUE(form_data_importer_->imported_credit_card_record_type_ ==
              FormDataImporter::ImportedCreditCardRecordType::LOCAL_CARD);
}

// Ensures that |imported_credit_card_record_type_| is set correctly.
TEST_P(FormDataImporterTest,
       ImportFormData_ImportCreditCardRecordType_MaskedServerCard) {
  // Add a masked server card.
  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::MASKED_SERVER_CARD, "a123"));
  test::SetCreditCardInfo(&server_cards.back(), "Biggie Smalls",
                          "1111" /* Visa */, "01", "2999", "");
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);
  test::SetServerCreditCards(autofill_table_, server_cards);

  // Make sure everything is set up correctly.
  personal_data_manager_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_manager_->GetCreditCards().size());

  // Simulate a form submission with the same masked server card.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form, "Biggie Smalls", "4111 1111 1111 1111", "01",
                        "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  std::unique_ptr<CreditCard> imported_credit_card;
  absl::optional<std::string> imported_upi_id;
  EXPECT_FALSE(ImportFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/true, &imported_credit_card,
      &imported_upi_id));
  ASSERT_FALSE(imported_credit_card);
  // |imported_credit_card_record_type_| should be SERVER_CARD.
  ASSERT_TRUE(form_data_importer_->imported_credit_card_record_type_ ==
              FormDataImporter::ImportedCreditCardRecordType::SERVER_CARD);
}

// Ensures that |imported_credit_card_record_type_| is set correctly.
TEST_P(FormDataImporterTest,
       ImportFormData_ImportCreditCardRecordType_FullServerCard) {
  // Add a full server card.
  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::FULL_SERVER_CARD, "c789"));
  test::SetCreditCardInfo(&server_cards.back(), "Biggie Smalls",
                          "378282246310005" /* American Express */, "04",
                          "2999", "1");
  test::SetServerCreditCards(autofill_table_, server_cards);

  // Make sure everything is set up correctly.
  personal_data_manager_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_manager_->GetCreditCards().size());

  // Simulate a form submission with the same full server card.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form, "Biggie Smalls", "378282246310005", "04",
                        "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  std::unique_ptr<CreditCard> imported_credit_card;
  absl::optional<std::string> imported_upi_id;
  EXPECT_FALSE(ImportFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/true, &imported_credit_card,
      &imported_upi_id));
  ASSERT_FALSE(imported_credit_card);
  // |imported_credit_card_record_type_| should be SERVER_CARD.
  ASSERT_TRUE(form_data_importer_->imported_credit_card_record_type_ ==
              FormDataImporter::ImportedCreditCardRecordType::SERVER_CARD);
}

// Ensures that |imported_credit_card_record_type_| is set correctly.
TEST_P(FormDataImporterTest,
       ImportFormData_ImportCreditCardRecordType_NoCard_InvalidCardNumber) {
  // Simulate a form submission using a credit card with an invalid card number.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form, "Biggie Smalls", "4111 1111 1111 1112", "01",
                        "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  std::unique_ptr<CreditCard> imported_credit_card;
  absl::optional<std::string> imported_upi_id;
  EXPECT_FALSE(ImportFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/true, &imported_credit_card,
      &imported_upi_id));
  ASSERT_FALSE(imported_credit_card);
  // |imported_credit_card_record_type_| should be NO_CARD because no valid card
  // was successfully imported from the form.
  ASSERT_TRUE(form_data_importer_->imported_credit_card_record_type_ ==
              FormDataImporter::ImportedCreditCardRecordType::NO_CARD);
}

// Ensures that |imported_credit_card_record_type_| is set correctly.
TEST_P(FormDataImporterTest,
       ImportFormData_ImportCreditCardRecordType_NoCard_VirtualCard) {
  // Simulate a form submission using a credit card that is known as a virtual
  // card.
  FormData form;
  form.url = GURL("https://wwww.foo.com");
  AddFullCreditCardForm(&form, "Biggie Smalls", "4111 1111 1111 1111", "01",
                        "2999");
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  form_data_importer_->CacheFetchedVirtualCard(u"1111");
  std::unique_ptr<CreditCard> imported_credit_card;
  absl::optional<std::string> imported_upi_id;

  EXPECT_FALSE(ImportFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/true, &imported_credit_card,
      &imported_upi_id));
  ASSERT_FALSE(imported_credit_card);
  // |imported_credit_card_record_type_| should be NO_CARD because the card
  // imported from the form was a virtual card.
  ASSERT_TRUE(form_data_importer_->imported_credit_card_record_type_ ==
              FormDataImporter::ImportedCreditCardRecordType::NO_CARD);
}

// Ensures that |imported_credit_card_record_type_| is set correctly.
TEST_P(
    FormDataImporterTest,
    ImportFormData_ImportCreditCardRecordType_NewCard_ExpiredCard_WithExpDateFixFlow) {
  // Simulate a form submission with an expired credit card.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form, "Biggie Smalls", "4111 1111 1111 1111", "01",
                        "1999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  std::unique_ptr<CreditCard> imported_credit_card;
  absl::optional<std::string> imported_upi_id;
  EXPECT_TRUE(ImportFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/true, &imported_credit_card,
      &imported_upi_id));
  ASSERT_TRUE(imported_credit_card);
  // |imported_credit_card_record_type_| should be NEW_CARD because card was
  // successfully imported from the form via the expiration date fix flow.
  ASSERT_TRUE(form_data_importer_->imported_credit_card_record_type_ ==
              FormDataImporter::ImportedCreditCardRecordType::NEW_CARD);
}

// Ensures that |imported_credit_card_record_type_| is set correctly.
TEST_P(FormDataImporterTest,
       ImportFormData_ImportCreditCardRecordType_NoCard_NoCardOnForm) {
  // Simulate a form submission with no credit card on form.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("First name:", "first_name", "George", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Washington", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "bogus@example.com", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "21 Laussat St", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  std::unique_ptr<CreditCard> imported_credit_card;
  absl::optional<std::string> imported_upi_id;
  EXPECT_TRUE(ImportFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/true, &imported_credit_card,
      &imported_upi_id));
  ASSERT_FALSE(imported_credit_card);
  // |imported_credit_card_record_type_| should be NO_CARD because the form
  // doesn't have credit card section.
  ASSERT_TRUE(form_data_importer_->imported_credit_card_record_type_ ==
              FormDataImporter::ImportedCreditCardRecordType::NO_CARD);
}

// ImportFormData tests (both addresses and credit cards).

// Test that a form with both address and credit card sections imports the
// address and the credit card.
TEST_P(FormDataImporterTest, ImportFormData_OneAddressOneCreditCard) {
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  // Address section.
  test::CreateTestFormField("First name:", "first_name", "George", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Washington", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "theprez@gmail.com", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "21 Laussat St", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);

  // Credit card section.
  AddFullCreditCardForm(&form, "Biggie Smalls", "4111-1111-1111-1111", "01",
                        "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  std::unique_ptr<CreditCard> imported_credit_card;
  absl::optional<std::string> imported_upi_id;
  EXPECT_TRUE(ImportFormDataAndProcessAddressCandidates(
      form_structure,
      /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/false, &imported_credit_card,
      &imported_upi_id));
  ASSERT_TRUE(imported_credit_card);
  personal_data_manager_->OnAcceptedLocalCreditCardSave(*imported_credit_card);

  WaitForOnPersonalDataChanged();

  // Test that the address has been saved.
  AutofillProfile expected_address(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&expected_address, "George", nullptr, "Washington",
                       "theprez@gmail.com", nullptr, "21 Laussat St", nullptr,
                       "San Francisco", "California", "94102", nullptr,
                       nullptr);
  const std::vector<AutofillProfile*>& results_addr =
      personal_data_manager_->GetProfiles();
  ASSERT_EQ(1U, results_addr.size());
  EXPECT_EQ(0, expected_address.Compare(*results_addr[0]));

  // Test that the credit card has also been saved.
  CreditCard expected_card(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected_card, "Biggie Smalls", "4111111111111111",
                          "01", "2999", "");
  const std::vector<CreditCard*>& results_cards =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results_cards.size());
  EXPECT_EQ(0, expected_card.Compare(*results_cards[0]));
}

// Test that a form with two address sections and a credit card section does not
// import the address but does import the credit card.
TEST_P(FormDataImporterTest, ImportFormData_TwoAddressesOneCreditCard) {
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  // Address section 1.
  test::CreateTestFormField("First name:", "first_name", "George", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Washington", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "theprez@gmail.com", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "21 Laussat St", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);

  // Address section 2.
  test::CreateTestFormField("Address:", "address", "1600 Pennsylvania Avenue",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Name:", "name", "Barack Obama", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "Washington", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "DC", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "20500", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Country:", "country", "USA", "text", &field);
  form.fields.push_back(field);

  // Credit card section.
  AddFullCreditCardForm(&form, "Biggie Smalls", "4111-1111-1111-1111", "01",
                        "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  std::unique_ptr<CreditCard> imported_credit_card;

  base::RunLoop run_loop;
  EXPECT_CALL(personal_data_observer_, OnPersonalDataFinishedProfileTasks())
      .WillRepeatedly(QuitMessageLoop(&run_loop));
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .Times(testing::AnyNumber());
  absl::optional<std::string> imported_upi_id;
  // Still returns true because the credit card import was successful.
  EXPECT_TRUE(ImportFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/false, &imported_credit_card,
      &imported_upi_id));
  run_loop.Run();

  ASSERT_TRUE(imported_credit_card);
  personal_data_manager_->OnAcceptedLocalCreditCardSave(*imported_credit_card);

  WaitForOnPersonalDataChanged();

  // Test that both addresses have been saved.
  EXPECT_EQ(2U, personal_data_manager_->GetProfiles().size());

  // Test that the credit card has been saved.
  CreditCard expected_card(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected_card, "Biggie Smalls", "4111111111111111",
                          "01", "2999", "");
  const std::vector<CreditCard*>& results =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected_card.Compare(*results[0]));
}

// Test that a form is split into two sections correctly when the name has
// a different input format between the sections.
TEST_P(FormDataImporterTest, ImportFormData_TwoAddressesNameFirst) {
  base::test::ScopedFeatureList redundant_name_sectioning_feature;
  redundant_name_sectioning_feature.InitAndEnableFeature(
      features::kAutofillSectionUponRedundantNameInfo);

  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  // Address section 1.
  test::CreateTestFormField("First name:", "first_name", "George", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Washington", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "theprez@gmail.com", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "21 Laussat St", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);

  // Address section 2.
  // The sectioning should start a new section when a name
  // follows a FIRST_NAME + LAST_NAME field.
  test::CreateTestFormField("Name:", "name", "Barack Obama", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address", "1600 Pennsylvania Avenue",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "Washington", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "DC", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "20500", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Country:", "country", "USA", "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  ImportAddressProfiles(/*extraction_successful=*/true, form_structure);

  // Test that both addresses have been saved.
  EXPECT_EQ(2U, personal_data_manager_->GetProfiles().size());
}

// Test that a form with both address and credit card sections imports only the
// the credit card if addresses are disabled.
TEST_P(FormDataImporterTest, ImportFormData_AddressesDisabledOneCreditCard) {
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  // Address section.
  test::CreateTestFormField("First name:", "first_name", "George", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Washington", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "theprez@gmail.com", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "21 Laussat St", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);

  // Credit card section.
  AddFullCreditCardForm(&form, "Biggie Smalls", "4111-1111-1111-1111", "01",
                        "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  std::unique_ptr<CreditCard> imported_credit_card;
  absl::optional<std::string> imported_upi_id;
  EXPECT_TRUE(ImportFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/false,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/false, &imported_credit_card,
      &imported_upi_id));
  ASSERT_TRUE(imported_credit_card);
  personal_data_manager_->OnAcceptedLocalCreditCardSave(*imported_credit_card);

  WaitForOnPersonalDataChanged();

  // Test that addresses were not saved.
  EXPECT_EQ(0U, personal_data_manager_->GetProfiles().size());

  // Test that the credit card has been saved.
  CreditCard expected_card(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected_card, "Biggie Smalls", "4111111111111111",
                          "01", "2999", "");
  const std::vector<CreditCard*>& results =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected_card.Compare(*results[0]));
}

// Test that a form with both address and credit card sections imports only the
// the address if credit cards are disabled.
TEST_P(FormDataImporterTest, ImportFormData_OneAddressCreditCardDisabled) {
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  // Address section.
  test::CreateTestFormField("First name:", "first_name", "George", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Washington", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "theprez@gmail.com", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "21 Laussat St", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);

  // Credit card section.
  AddFullCreditCardForm(&form, "Biggie Smalls", "4111-1111-1111-1111", "01",
                        "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  std::unique_ptr<CreditCard> imported_credit_card;
  absl::optional<std::string> imported_upi_id;
  EXPECT_TRUE(ImportFormDataAndProcessAddressCandidates(
      form_structure,
      /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/false,
      /*should_return_local_card=*/false, &imported_credit_card,
      &imported_upi_id));
  ASSERT_FALSE(imported_credit_card);

  WaitForOnPersonalDataChanged();

  // Test that the address has been saved.
  AutofillProfile expected_address(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&expected_address, "George", nullptr, "Washington",
                       "theprez@gmail.com", nullptr, "21 Laussat St", nullptr,
                       "San Francisco", "California", "94102", nullptr,
                       nullptr);
  const std::vector<AutofillProfile*>& results_addr =
      personal_data_manager_->GetProfiles();
  ASSERT_EQ(1U, results_addr.size());
  EXPECT_EQ(0, expected_address.Compare(*results_addr[0]));

  // Test that the credit card was not saved.
  const std::vector<CreditCard*>& results_cards =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(0U, results_cards.size());
}

// Test that a form with both address and credit card sections imports nothing
// if both addressed and credit cards are disabled.
TEST_P(FormDataImporterTest, ImportFormData_AddressCreditCardDisabled) {
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  // Address section.
  test::CreateTestFormField("First name:", "first_name", "George", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Washington", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "theprez@gmail.com", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "21 Laussat St", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);

  // Credit card section.
  AddFullCreditCardForm(&form, "Biggie Smalls", "4111-1111-1111-1111", "01",
                        "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  std::unique_ptr<CreditCard> imported_credit_card;
  absl::optional<std::string> imported_upi_id;
  EXPECT_FALSE(ImportFormDataAndProcessAddressCandidates(
      form_structure,
      /*profile_autofill_enabled=*/false,
      /*credit_card_autofill_enabled=*/false,
      /*should_return_local_card=*/false, &imported_credit_card,
      &imported_upi_id));
  ASSERT_FALSE(imported_credit_card);

  // Test that addresses were not saved.
  EXPECT_EQ(0U, personal_data_manager_->GetProfiles().size());

  // Test that the credit card was not saved.
  const std::vector<CreditCard*>& results_cards =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(0U, results_cards.size());
}

TEST_P(FormDataImporterTest, DontDuplicateMaskedServerCard) {
  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::MASKED_SERVER_CARD, "a123"));
  test::SetCreditCardInfo(&server_cards.back(), "John Dillinger",
                          "1881" /* Visa */, "01", "2999", "");
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);

  server_cards.push_back(CreditCard(CreditCard::FULL_SERVER_CARD, "c789"));
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2999", "");

  test::SetServerCreditCards(autofill_table_, server_cards);

  // Make sure everything is set up correctly.
  personal_data_manager_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(2U, personal_data_manager_->GetCreditCards().size());

  // A valid credit card form. A user re-enters one of their masked cards.
  // We should not offer to save locally.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name on card:", "name_on_card", "John Dillinger",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number:", "card_number", "4012888888881881",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "01", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2999", "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  std::unique_ptr<CreditCard> imported_credit_card;
  absl::optional<std::string> imported_upi_id;
  EXPECT_FALSE(ImportFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/false, &imported_credit_card,
      &imported_upi_id));
  ASSERT_FALSE(imported_credit_card);
}

// Tests that a credit card form that is hidden after receiving input still
// imports the card.
TEST_P(FormDataImporterTest, ImportFormData_HiddenCreditCardFormAfterEntered) {
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;

  test::CreateTestFormField("Name on card:", "name_on_card", "Biggie Smalls",
                            "text", &field);
  field.is_focusable = false;
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number:", "card_number", "4111111111111111",
                            "text", &field);
  field.is_focusable = false;
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "theprez@gmail.com", "text",
                            &field);
  field.is_focusable = false;
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "01", "text", &field);
  field.is_focusable = false;
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2999", "text", &field);
  field.is_focusable = false;
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  std::unique_ptr<CreditCard> imported_credit_card;
  absl::optional<std::string> imported_upi_id;
  // Still returns true because the credit card import was successful.
  EXPECT_TRUE(ImportFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/false, &imported_credit_card,
      &imported_upi_id));
  ASSERT_TRUE(imported_credit_card);
  personal_data_manager_->OnAcceptedLocalCreditCardSave(*imported_credit_card);

  WaitForOnPersonalDataChanged();

  // Test that the credit card has been saved.
  CreditCard expected_card(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected_card, "Biggie Smalls", "4111111111111111",
                          "01", "2999", "");
  const std::vector<CreditCard*>& results =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected_card.Compare(*results[0]));
}

// Ensures that no UPI ID value is returned when there's a credit card and no
// UPI ID.
TEST_P(FormDataImporterTest,
       ImportFormData_DontSetUpiIdWhenOnlyCreditCardExists) {
  // Simulate a form submission with a new credit card.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form, "Biggie Smalls", "4111 1111 1111 1111", "01",
                        "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  std::unique_ptr<CreditCard> imported_credit_card;
  absl::optional<std::string> imported_upi_id;
  EXPECT_TRUE(ImportFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/true, &imported_credit_card,
      &imported_upi_id));
  ASSERT_FALSE(imported_upi_id.has_value());
}

TEST_P(FormDataImporterTest, DontDuplicateFullServerCard) {
  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::MASKED_SERVER_CARD, "a123"));
  test::SetCreditCardInfo(&server_cards.back(), "John Dillinger",
                          "1881" /* Visa */, "01", "2999", "1");
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);

  server_cards.push_back(CreditCard(CreditCard::FULL_SERVER_CARD, "c789"));
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2999", "1");

  test::SetServerCreditCards(autofill_table_, server_cards);

  // Make sure everything is set up correctly.
  personal_data_manager_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(2U, personal_data_manager_->GetCreditCards().size());

  // A user re-types (or fills with) an unmasked card. Don't offer to save
  // here, either. Since it's unmasked, we know for certain that it's the same
  // card.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name on card:", "name_on_card", "Clyde Barrow",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number:", "card_number", "378282246310005",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "04", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2999", "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  std::unique_ptr<CreditCard> imported_credit_card;
  absl::optional<std::string> imported_upi_id;
  EXPECT_FALSE(ImportFormDataAndProcessAddressCandidates(
      form_structure,
      /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/false, &imported_credit_card,
      &imported_upi_id));
  EXPECT_FALSE(imported_credit_card);
}

TEST_P(FormDataImporterTest,
       Metrics_SubmittedServerCardExpirationStatus_FullServerCardMatch) {
  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::FULL_SERVER_CARD, "c789"));
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "4444333322221111" /* Visa */, "04", "2111", "1");

  test::SetServerCreditCards(autofill_table_, server_cards);

  // Make sure everything is set up correctly.
  personal_data_manager_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_manager_->GetCreditCards().size());

  // A user fills/enters the card's information on a checkout form.  Ensure that
  // an expiration date match is recorded.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name on card:", "name_on_card", "Clyde Barrow",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number:", "card_number", "4444333322221111",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "04", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2111", "text", &field);
  form.fields.push_back(field);

  base::HistogramTester histogram_tester;
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  std::unique_ptr<CreditCard> imported_credit_card;
  absl::optional<std::string> imported_upi_id;
  EXPECT_FALSE(ImportFormDataAndProcessAddressCandidates(
      form_structure,
      /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/false, &imported_credit_card,
      &imported_upi_id));
  EXPECT_FALSE(imported_credit_card);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SubmittedServerCardExpirationStatus",
      AutofillMetrics::FULL_SERVER_CARD_EXPIRATION_DATE_MATCHED, 1);
}

// Ensure that we don't offer to save if we already have same card stored as a
// server card and user submitted an invalid expiration date month.
TEST_P(FormDataImporterTest,
       Metrics_SubmittedServerCardExpirationStatus_EmptyExpirationMonth) {
  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::FULL_SERVER_CARD, "c789"));
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "4444333322221111" /* Visa */, "04", "2111", "1");

  test::SetServerCreditCards(autofill_table_, server_cards);

  // Make sure everything is set up correctly.
  personal_data_manager_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_manager_->GetCreditCards().size());

  // A user fills/enters the card's information on a checkout form with an empty
  // expiration date.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name on card:", "name_on_card", "Clyde Barrow",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number:", "card_number", "4444333322221111",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2111", "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  std::unique_ptr<CreditCard> imported_credit_card;
  absl::optional<std::string> imported_upi_id;
  EXPECT_FALSE(ImportFormDataAndProcessAddressCandidates(
      form_structure,
      /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/false, &imported_credit_card,
      &imported_upi_id));
  EXPECT_FALSE(imported_credit_card);
}

// Ensure that we don't offer to save if we already have same card stored as a
// server card and user submitted an invalid expiration date year.
TEST_P(FormDataImporterTest,
       Metrics_SubmittedServerCardExpirationStatus_EmptyExpirationYear) {
  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::FULL_SERVER_CARD, "c789"));
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "4444333322221111" /* Visa */, "04", "2111", "1");

  test::SetServerCreditCards(autofill_table_, server_cards);

  // Make sure everything is set up correctly.
  personal_data_manager_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_manager_->GetCreditCards().size());

  // A user fills/enters the card's information on a checkout form with an empty
  // expiration date.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name on card:", "name_on_card", "Clyde Barrow",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number:", "card_number", "4444333322221111",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "08", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "", "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  std::unique_ptr<CreditCard> imported_credit_card;
  absl::optional<std::string> imported_upi_id;
  EXPECT_FALSE(ImportFormDataAndProcessAddressCandidates(
      form_structure,
      /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/false, &imported_credit_card,
      &imported_upi_id));
  EXPECT_FALSE(imported_credit_card);
}

// Ensure that we still offer to save if we have different cards stored as a
// server card and user submitted an invalid expiration date year.
TEST_P(
    FormDataImporterTest,
    Metrics_SubmittedDifferentServerCardExpirationStatus_EmptyExpirationYear) {
  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::FULL_SERVER_CARD, "c789"));
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "4111111111111111" /* Visa */, "04", "2111", "1");

  test::SetServerCreditCards(autofill_table_, server_cards);

  // Make sure everything is set up correctly.
  personal_data_manager_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_manager_->GetCreditCards().size());

  // A user fills/enters the card's information on a checkout form with an empty
  // expiration date.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name on card:", "name_on_card", "Clyde Barrow",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number:", "card_number", "4444333322221111",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "08", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "", "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  std::unique_ptr<CreditCard> imported_credit_card;
  absl::optional<std::string> imported_upi_id;
  EXPECT_TRUE(ImportFormDataAndProcessAddressCandidates(
      form_structure,
      /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/false, &imported_credit_card,
      &imported_upi_id));
  EXPECT_TRUE(imported_credit_card);
}

TEST_P(FormDataImporterTest,
       Metrics_SubmittedServerCardExpirationStatus_FullServerCardMismatch) {
  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::FULL_SERVER_CARD, "c789"));
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "4444333322221111" /* Visa */, "04", "2111", "1");

  test::SetServerCreditCards(autofill_table_, server_cards);

  // Make sure everything is set up correctly.
  personal_data_manager_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_manager_->GetCreditCards().size());

  // A user fills/enters the card's information on a checkout form but changes
  // the expiration date of the card.  Ensure that an expiration date mismatch
  // is recorded.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name on card:", "name_on_card", "Clyde Barrow",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number:", "card_number", "4444333322221111",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "04", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2345", "text", &field);
  form.fields.push_back(field);

  base::HistogramTester histogram_tester;
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  std::unique_ptr<CreditCard> imported_credit_card;
  absl::optional<std::string> imported_upi_id;
  EXPECT_FALSE(ImportFormDataAndProcessAddressCandidates(
      form_structure,
      /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/false, &imported_credit_card,
      &imported_upi_id));
  EXPECT_FALSE(imported_credit_card);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SubmittedServerCardExpirationStatus",
      AutofillMetrics::FULL_SERVER_CARD_EXPIRATION_DATE_DID_NOT_MATCH, 1);
}

TEST_P(FormDataImporterTest,
       Metrics_SubmittedServerCardExpirationStatus_MaskedServerCardMatch) {
  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::MASKED_SERVER_CARD, "a123"));
  test::SetCreditCardInfo(&server_cards.back(), "John Dillinger",
                          "1111" /* Visa */, "01", "2111", "");
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);

  test::SetServerCreditCards(autofill_table_, server_cards);

  // Make sure everything is set up correctly.
  personal_data_manager_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_manager_->GetCreditCards().size());

  // A user fills/enters the card's information on a checkout form.  Ensure that
  // an expiration date match is recorded.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name on card:", "name_on_card", "Clyde Barrow",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number:", "card_number", "4444333322221111",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "01", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2111", "text", &field);
  form.fields.push_back(field);

  base::HistogramTester histogram_tester;
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  std::unique_ptr<CreditCard> imported_credit_card;
  absl::optional<std::string> imported_upi_id;
  EXPECT_FALSE(ImportFormDataAndProcessAddressCandidates(
      form_structure,
      /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/false, &imported_credit_card,
      &imported_upi_id));
  EXPECT_FALSE(imported_credit_card);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SubmittedServerCardExpirationStatus",
      AutofillMetrics::MASKED_SERVER_CARD_EXPIRATION_DATE_MATCHED, 1);
}

TEST_P(FormDataImporterTest,
       Metrics_SubmittedServerCardExpirationStatus_MaskedServerCardMismatch) {
  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::MASKED_SERVER_CARD, "a123"));
  test::SetCreditCardInfo(&server_cards.back(), "John Dillinger",
                          "1111" /* Visa */, "01", "2111", "");
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);

  test::SetServerCreditCards(autofill_table_, server_cards);

  // Make sure everything is set up correctly.
  personal_data_manager_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_manager_->GetCreditCards().size());

  // A user fills/enters the card's information on a checkout form but changes
  // the expiration date of the card.  Ensure that an expiration date mismatch
  // is recorded.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name on card:", "name_on_card", "Clyde Barrow",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number:", "card_number", "4444333322221111",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "04", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2345", "text", &field);
  form.fields.push_back(field);

  base::HistogramTester histogram_tester;
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  std::unique_ptr<CreditCard> imported_credit_card;
  absl::optional<std::string> imported_upi_id;
  EXPECT_FALSE(ImportFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/false, &imported_credit_card,
      &imported_upi_id));
  EXPECT_FALSE(imported_credit_card);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SubmittedServerCardExpirationStatus",
      AutofillMetrics::MASKED_SERVER_CARD_EXPIRATION_DATE_DID_NOT_MATCH, 1);
}

TEST_P(FormDataImporterTest, ImportUpiId) {
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("UPI ID:", "upi_id", "user@indianbank", "text",
                            &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);

  std::unique_ptr<CreditCard> imported_credit_card;  // Discarded.
  absl::optional<std::string> imported_upi_id;

  EXPECT_TRUE(ImportFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/false,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/false, &imported_credit_card,
      &imported_upi_id));

  ASSERT_TRUE(imported_upi_id.has_value());
  EXPECT_EQ(imported_upi_id.value(), "user@indianbank");
}

TEST_P(FormDataImporterTest, ImportUpiIdDisabled) {
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("UPI ID:", "upi_id", "user@indianbank", "text",
                            &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);

  std::unique_ptr<CreditCard> imported_credit_card;  // Discarded.
  absl::optional<std::string> imported_upi_id;

  EXPECT_FALSE(ImportFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/false,
      /*credit_card_autofill_enabled=*/false,
      /*should_return_local_card=*/false, &imported_credit_card,
      &imported_upi_id));

  EXPECT_FALSE(imported_upi_id.has_value());
}

TEST_P(FormDataImporterTest, ImportUpiIdIgnoreNonUpiId) {
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("UPI ID:", "upi_id", "user@gmail.com", "text",
                            &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);

  std::unique_ptr<CreditCard> imported_credit_card;  // Discarded.
  absl::optional<std::string> imported_upi_id;

  EXPECT_FALSE(ImportFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/false,
      /*credit_card_autofill_enabled=*/false,
      /*should_return_local_card=*/false, &imported_credit_card,
      &imported_upi_id));

  EXPECT_FALSE(imported_upi_id.has_value());
}

TEST_P(FormDataImporterTest, SilentlyUpdateExistingProfileByIncompleteProfile) {
  // This test is only applicable when structured names are enabled.
  if (!StructuredNames())
    return;

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillSilentProfileUpdateForInsufficientImport);

  AutofillProfile profile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");

  // Set the verification status for the first and middle name to parsed.
  profile.SetRawInfoWithVerificationStatus(
      NAME_FIRST, u"Marion", structured_address::VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(
      NAME_MIDDLE, u"Mitchell",
      structured_address::VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(
      NAME_LAST, u"Morrison", structured_address::VerificationStatus::kParsed);

  base::RunLoop run_loop;
  EXPECT_CALL(personal_data_observer_, OnPersonalDataFinishedProfileTasks())
      .WillOnce(QuitMessageLoop(&run_loop));
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged()).Times(1);
  personal_data_manager_->AddProfile(profile);
  run_loop.Run();

  // Simulate a form submission with conflicting info.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("First name:", "first_name", "Marion", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Middle name:", "middle_name", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Mitchell Morrison",
                            "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  ImportAddressProfiles(/*extraction_successful=*/false, form_structure);

  // Expect that no new profile is saved.
  const std::vector<AutofillProfile*>& results =
      personal_data_manager_->GetProfiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(1, profile.Compare(*results[0]));
  EXPECT_EQ(results[0]->GetRawInfo(NAME_FULL), u"Marion Mitchell Morrison");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_FIRST), u"Marion");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_MIDDLE), u"");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_LAST), u"Mitchell Morrison");
}

TEST_P(
    FormDataImporterTest,
    SilentlyUpdateExistingProfileByIncompleteProfile_DespiteDisallowedPrompts) {
  // This test is only applicable when structured names are enabled.
  if (!StructuredNames())
    return;

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kAutofillSilentProfileUpdateForInsufficientImport,
       features::kAutofillAddressProfileSavePrompt},
      {});

  AutofillProfile profile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");

  // Set the verification status for the first and middle name to parsed.
  profile.SetRawInfoWithVerificationStatus(
      NAME_FIRST, u"Marion", structured_address::VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(
      NAME_MIDDLE, u"Mitchell",
      structured_address::VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(
      NAME_LAST, u"Morrison", structured_address::VerificationStatus::kParsed);

  base::RunLoop run_loop;
  EXPECT_CALL(personal_data_observer_, OnPersonalDataFinishedProfileTasks())
      .WillOnce(QuitMessageLoop(&run_loop));
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged()).Times(1);
  personal_data_manager_->AddProfile(profile);
  run_loop.Run();

  // Simulate a form submission with conflicting info.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("First name:", "first_name", "Marion", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Middle name:", "middle_name", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Mitchell Morrison",
                            "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  ImportAddressProfiles(/*extraction_successful=*/false, form_structure,
                        /*skip_waiting_on_pdm=*/false,
                        /*allow_save_prompts=*/false);

  // Expect that no new profile is saved and the existing profile is updated.
  const std::vector<AutofillProfile*>& results =
      personal_data_manager_->GetProfiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(1, profile.Compare(*results[0]));
  EXPECT_EQ(results[0]->GetRawInfo(NAME_FULL), u"Marion Mitchell Morrison");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_FIRST), u"Marion");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_MIDDLE), u"");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_LAST), u"Mitchell Morrison");
}

TEST_P(FormDataImporterTest, UnusableIncompleteProfile) {
  // This test is only applicable when structured names are enabled.
  if (!StructuredNames())
    return;

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillSilentProfileUpdateForInsufficientImport);

  AutofillProfile profile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");

  // Set the verification status for the first and middle name to parsed.
  profile.SetRawInfoWithVerificationStatus(
      NAME_FIRST, u"Marion", structured_address::VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(
      NAME_MIDDLE, u"Mitchell",
      structured_address::VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(
      NAME_LAST, u"Morrison", structured_address::VerificationStatus::kParsed);

  base::RunLoop run_loop;
  EXPECT_CALL(personal_data_observer_, OnPersonalDataFinishedProfileTasks())
      .WillOnce(QuitMessageLoop(&run_loop));
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged()).Times(1);
  personal_data_manager_->AddProfile(profile);
  run_loop.Run();

  // Simulate a form submission with conflicting info.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("First name:", "first_name", "Marion", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Middle name:", "middle_name", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Mitch Morrison", "text",
                            &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  ImportAddressProfiles(/*extraction_successful=*/false, form_structure,
                        /*skip_waiting_on_pdm=*/true);

  // Expect that no new profile is saved.
  const std::vector<AutofillProfile*>& results =
      personal_data_manager_->GetProfiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, profile.Compare(*results[0]));
  EXPECT_EQ(results[0]->GetRawInfo(NAME_FULL), u"Marion Mitchell Morrison");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_FIRST), u"Marion");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_MIDDLE), u"Mitchell");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_LAST), u"Morrison");
}

// Runs the suite with the feature |kAutofillSupportForMoreStructuredNames|,
// |kAutofillSupportForMoreStructuredAddresses| and
// |kAutofillEnableSupportForApartmentNumbers| enabled and disabled.
INSTANTIATE_TEST_SUITE_P(,
                         FormDataImporterTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));

}  // namespace autofill
