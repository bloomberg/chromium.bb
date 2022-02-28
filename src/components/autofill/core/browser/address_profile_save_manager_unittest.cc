// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/address_profile_save_manager.h"

#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_test_utils.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/test_utils/test_profiles.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using UserDecision = AutofillClient::SaveAddressProfileOfferUserDecision;

using structured_address::VerificationStatus;

// Names of histrogram used for metric collection.
constexpr char kProfileImportTypeHistogram[] =
    "Autofill.ProfileImport.ProfileImportType";
constexpr char kSilentUpdatesProfileImportTypeHistogram[] =
    "Autofill.ProfileImport.SilentUpdatesProfileImportType";
constexpr char kNewProfileEditsHistogram[] =
    "Autofill.ProfileImport.NewProfileEditedType";
constexpr char kProfileUpdateEditsHistogram[] =
    "Autofill.ProfileImport.UpdateProfileEditedType";
constexpr char kProfileUpdateAffectedTypesHistogram[] =
    "Autofill.ProfileImport.UpdateProfileAffectedType";
constexpr char kNewProfileDecisionHistogram[] =
    "Autofill.ProfileImport.NewProfileDecision";
constexpr char kProfileUpdateDecisionHistogram[] =
    "Autofill.ProfileImport.UpdateProfileDecision";
constexpr char kNewProfileNumberOfEditsHistogram[] =
    "Autofill.ProfileImport.NewProfileNumberOfEditedFields";
constexpr char kProfileUpdateNumberOfEditsHistogram[] =
    "Autofill.ProfileImport.UpdateProfileNumberOfEditedFields";
constexpr char kProfileUpdateNumberOfAffectedTypesHistogram[] =
    "Autofill.ProfileImport.UpdateProfileNumberOfAffectedFields";

class MockPersonalDataManager : public TestPersonalDataManager {
 public:
  MockPersonalDataManager() = default;
  ~MockPersonalDataManager() override = default;

  MOCK_METHOD(std::string,
              SaveImportedProfile,
              (const AutofillProfile&),
              (override));
};

// This derived version of the AddressProfileSaveManager stores the last import
// for testing purposes and mocks the UI request.
class TestAddressProfileSaveManager : public AddressProfileSaveManager {
 public:
  // The parameters should outlive the AddressProfileSaveManager.
  TestAddressProfileSaveManager(AutofillClient* client,
                                PersonalDataManager* personal_data_manager);

  // Mocks the function that initiates the UI prompt for testing purposes.
  MOCK_METHOD(void,
              OfferSavePrompt,
              (std::unique_ptr<ProfileImportProcess>),
              (override));

  // Returns a copy of the last finished import process or 'absl::nullopt' if no
  // import process was finished.
  ProfileImportProcess* last_import();

  void OnUserDecisionForTesting(
      std::unique_ptr<ProfileImportProcess> import_process,
      UserDecision decision,
      AutofillProfile edited_profile) {
    if (profile_added_while_waiting_for_user_response_) {
      personal_data_manager()->AddProfile(
          profile_added_while_waiting_for_user_response_.value());
    }

    import_process->set_prompt_was_shown();
    OnUserDecision(std::move(import_process), decision, edited_profile);
  }

  void SetProfileThatIsAddedInWhileWaitingForUserResponse(
      const AutofillProfile& profile) {
    profile_added_while_waiting_for_user_response_ = profile;
  }

 protected:
  void ClearPendingImport(
      std::unique_ptr<ProfileImportProcess> import_process) override;
  // Profile that is passed from the emulated UI respones in case the user
  // edited the import candidate.
  std::unique_ptr<ProfileImportProcess> last_import_;

  // If set, this is a profile that is added in between the import operation
  // while the response from the user is pending.
  absl::optional<AutofillProfile>
      profile_added_while_waiting_for_user_response_;
};

TestAddressProfileSaveManager::TestAddressProfileSaveManager(
    AutofillClient* client,
    PersonalDataManager* personal_data_manager)
    : AddressProfileSaveManager(client, personal_data_manager) {}

void TestAddressProfileSaveManager::ClearPendingImport(
    std::unique_ptr<ProfileImportProcess> import_process) {
  last_import_ = std::move(import_process);
  AddressProfileSaveManager::ClearPendingImport(std::move(import_process));
}

ProfileImportProcess* TestAddressProfileSaveManager::last_import() {
  return last_import_.get();
}

// Definition of a test scenario.
struct ImportScenarioTestCase {
  std::vector<AutofillProfile> existing_profiles;
  AutofillProfile observed_profile;
  bool is_prompt_expected;
  UserDecision user_decision;
  AutofillProfile edited_profile;
  AutofillProfileImportType expected_import_type;
  bool is_profile_change_expected;
  absl::optional<AutofillProfile> merge_candidate;
  absl::optional<AutofillProfile> import_candidate;
  std::vector<AutofillProfile> expected_final_profiles;
  std::vector<AutofillMetrics::SettingsVisibleFieldTypeForMetrics>
      expected_edited_types_for_metrics;
  std::vector<AutofillMetrics::SettingsVisibleFieldTypeForMetrics>
      expected_affeceted_types_in_merge_for_metrics;
  bool new_profiles_suppresssed_for_domain;
  std::vector<std::string> blocked_guids_for_updates;
  absl::optional<AutofillProfile> profile_to_be_added_while_waiting;
  bool allow_only_silent_updates = false;
};

class AddressProfileSaveManagerTest : public testing::Test {
 public:
  void SetUp() override {
    // Enable both explicit save prompts and structured names.
    // The latter is needed to test the concept of silent updates.
    scoped_feature_list_.InitWithFeatures(
        {features::kAutofillAddressProfileSavePrompt,
         features::kAutofillEnableSupportForMoreStructureInNames},
        {});
  }

  void BlockProfileForUpdates(const std::string& guid) {
    while (!mock_personal_data_manager_.IsProfileUpdateBlocked(guid)) {
      mock_personal_data_manager_.AddStrikeToBlockProfileUpdate(guid);
    }
  }

  // Tests the |test_scenario|.
  void TestImportScenario(ImportScenarioTestCase& test_scenario);

 protected:
  base::test::TaskEnvironment task_environment_;
  TestAutofillClient autofill_client_;
  MockPersonalDataManager mock_personal_data_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

void AddressProfileSaveManagerTest::TestImportScenario(
    ImportScenarioTestCase& test_scenario) {
  static const GURL url("https://www.importmyform.com/index.html");

  // Assert that there is not a single profile stored in the personal data
  // manager.
  ASSERT_TRUE(mock_personal_data_manager_.GetProfiles().empty());

  TestAddressProfileSaveManager save_manager(&autofill_client_,
                                             &mock_personal_data_manager_);
  base::HistogramTester histogram_tester;

  if (test_scenario.profile_to_be_added_while_waiting) {
    save_manager.SetProfileThatIsAddedInWhileWaitingForUserResponse(
        test_scenario.profile_to_be_added_while_waiting.value());
  }

  // If the domain is blocked for new imports, use the defined limit for the
  // initial strikes. Otherwise, use 1.
  int initial_strikes =
      test_scenario.new_profiles_suppresssed_for_domain
          ? mock_personal_data_manager_.GetProfileSaveStrikeDatabase()
                ->GetMaxStrikesLimit()
          : 1;
  mock_personal_data_manager_.GetProfileSaveStrikeDatabase()->AddStrikes(
      initial_strikes, url.host());
  ASSERT_EQ(mock_personal_data_manager_.IsNewProfileImportBlockedForDomain(url),
            test_scenario.new_profiles_suppresssed_for_domain);

  // Add one strike for each existing profile and the maximum number of strikes
  // for blocked profiles.
  for (const AutofillProfile& profile : test_scenario.existing_profiles) {
    mock_personal_data_manager_.AddStrikeToBlockProfileUpdate(profile.guid());
  }
  for (const std::string& guid : test_scenario.blocked_guids_for_updates) {
    BlockProfileForUpdates(guid);
  }

  // Set up the expectation and response for if a prompt should be shown.
  if (test_scenario.is_prompt_expected) {
    EXPECT_CALL(save_manager, OfferSavePrompt(testing::_))
        .Times(1)
        .WillOnce(testing::WithArgs<0>(
            [&](std::unique_ptr<ProfileImportProcess> import_process) {
              save_manager.OnUserDecisionForTesting(
                  std::move(import_process), test_scenario.user_decision,
                  test_scenario.edited_profile);
            }));
  } else {
    EXPECT_CALL(save_manager, OfferSavePrompt).Times(0);
  }

  // Set the existing profiles to the personal data manager.
  mock_personal_data_manager_.SetProfiles(&test_scenario.existing_profiles);

  // Initiate the profile import.
  save_manager.ImportProfileFromForm(
      test_scenario.observed_profile, "en-US", url,
      /*allow_only_silent_updates=*/test_scenario.allow_only_silent_updates);

  // Assert that there is a finished import process on record.
  ASSERT_NE(save_manager.last_import(), nullptr);
  ProfileImportProcess* last_import = save_manager.last_import();

  EXPECT_EQ(test_scenario.expected_import_type, last_import->import_type());

  // Make a copy of the final profiles in the personal data manager for
  // comparison.
  std::vector<AutofillProfile> final_profiles;
  final_profiles.reserve(test_scenario.expected_final_profiles.size());
  for (const auto* profile : mock_personal_data_manager_.GetProfiles())
    final_profiles.push_back(*profile);

  EXPECT_THAT(test_scenario.expected_final_profiles,
              testing::UnorderedElementsAreArray(final_profiles));

  // Test that the merge and import candidates are correct.
  EXPECT_EQ(test_scenario.merge_candidate, last_import->merge_candidate());
  EXPECT_EQ(test_scenario.import_candidate, last_import->import_candidate());

  // Test the collection of metrics.
  histogram_tester.ExpectUniqueSample(
      test_scenario.allow_only_silent_updates
          ? kSilentUpdatesProfileImportTypeHistogram
          : kProfileImportTypeHistogram,
      test_scenario.expected_import_type, 1);

  const bool is_new_profile = test_scenario.expected_import_type ==
                              AutofillProfileImportType::kNewProfile;
  const bool is_confirmable_merge =
      test_scenario.expected_import_type ==
          AutofillProfileImportType::kConfirmableMerge ||
      test_scenario.expected_import_type ==
          AutofillProfileImportType::kConfirmableMergeAndSilentUpdate;

  // If the import was neither a new profile or a confirmable merge, test that
  // the corresponding updates are unchanged.
  if (!is_new_profile && !is_confirmable_merge) {
    histogram_tester.ExpectTotalCount(kNewProfileEditsHistogram, 0);
    histogram_tester.ExpectTotalCount(kNewProfileDecisionHistogram, 0);
    histogram_tester.ExpectTotalCount(kProfileUpdateEditsHistogram, 0);
    histogram_tester.ExpectTotalCount(kProfileUpdateDecisionHistogram, 0);
  } else {
    DCHECK(!is_new_profile || !is_confirmable_merge);

    const std::string affected_decision_histo =
        is_new_profile ? kNewProfileDecisionHistogram
                       : kProfileUpdateDecisionHistogram;
    const std::string unaffected_decision_histo =
        !is_new_profile ? kNewProfileDecisionHistogram
                        : kProfileUpdateDecisionHistogram;

    const std::string affected_edits_histo = is_new_profile
                                                 ? kNewProfileEditsHistogram
                                                 : kProfileUpdateEditsHistogram;
    const std::string unaffected_edits_histo =
        !is_new_profile ? kNewProfileEditsHistogram
                        : kProfileUpdateEditsHistogram;

    const std::string affected_number_of_edits_histo =
        is_new_profile ? kNewProfileNumberOfEditsHistogram
                       : kProfileUpdateNumberOfEditsHistogram;
    const std::string unaffected_number_of_edits_histo =
        !is_new_profile ? kNewProfileNumberOfEditsHistogram
                        : kProfileUpdateNumberOfEditsHistogram;

    histogram_tester.ExpectTotalCount(unaffected_decision_histo, 0);
    histogram_tester.ExpectTotalCount(unaffected_edits_histo, 0);

    histogram_tester.ExpectUniqueSample(affected_decision_histo,
                                        test_scenario.user_decision, 1);
    histogram_tester.ExpectTotalCount(
        affected_edits_histo,
        test_scenario.expected_edited_types_for_metrics.size());

    for (auto edited_type : test_scenario.expected_edited_types_for_metrics) {
      histogram_tester.ExpectBucketCount(affected_edits_histo, edited_type, 1);
    }

    if (test_scenario.user_decision == UserDecision::kEditAccepted) {
      histogram_tester.ExpectUniqueSample(
          affected_number_of_edits_histo,
          test_scenario.expected_edited_types_for_metrics.size(), 1);
      histogram_tester.ExpectTotalCount(unaffected_number_of_edits_histo, 0);
    }

    if (is_confirmable_merge &&
        test_scenario.user_decision == UserDecision::kAccepted) {
      histogram_tester.ExpectTotalCount(
          kProfileUpdateAffectedTypesHistogram,
          test_scenario.expected_affeceted_types_in_merge_for_metrics.size());

      for (auto changed_type :
           test_scenario.expected_affeceted_types_in_merge_for_metrics) {
        histogram_tester.ExpectBucketCount(kProfileUpdateAffectedTypesHistogram,
                                           changed_type, 1);
        std::string changed_histogram_suffix;
        switch (test_scenario.user_decision) {
          case UserDecision::kAccepted:
            changed_histogram_suffix = ".Accepted";
            break;

          case UserDecision::kDeclined:
            changed_histogram_suffix = ".Declined";
            break;

          default:
            NOTREACHED() << "Decision not covered by test logic.";
        }
        histogram_tester.ExpectBucketCount(
            base::StrCat({kProfileUpdateAffectedTypesHistogram,
                          changed_histogram_suffix}),
            changed_type, 1);
      }

      histogram_tester.ExpectUniqueSample(
          kProfileUpdateNumberOfAffectedTypesHistogram,
          test_scenario.expected_affeceted_types_in_merge_for_metrics.size(),
          1);
    }
  }

  // Check that the strike count was incremented if the import of a new profile
  // was declined.
  if (is_new_profile && last_import->UserDeclined()) {
    EXPECT_EQ(2, mock_personal_data_manager_.GetProfileSaveStrikeDatabase()
                     ->GetStrikes(url.host()));
  } else if (is_new_profile && last_import->UserAccepted()) {
    // If the import of a new profile was accepted, the count should have been
    // reset.
    EXPECT_EQ(0, mock_personal_data_manager_.GetProfileSaveStrikeDatabase()
                     ->GetStrikes(url.host()));
  } else {
    // In all other cases, the number of strikes should be unaltered.
    EXPECT_EQ(
        initial_strikes,
        mock_personal_data_manager_.GetProfileSaveStrikeDatabase()->GetStrikes(
            url.host()));
  }

  // Check that the strike count for profile updates is reset if a profile was
  // updated.
  if (is_confirmable_merge &&
      (test_scenario.user_decision == UserDecision::kAccepted ||
       test_scenario.user_decision == UserDecision::kEditAccepted)) {
    EXPECT_EQ(0, mock_personal_data_manager_.GetProfileUpdateStrikeDatabase()
                     ->GetStrikes(test_scenario.merge_candidate->guid()));
  } else if (is_confirmable_merge &&
             (test_scenario.user_decision == UserDecision::kDeclined ||
              test_scenario.user_decision == UserDecision::kMessageDeclined)) {
    // Or that it is incremented if the update was declined.
    EXPECT_EQ(2, mock_personal_data_manager_.GetProfileUpdateStrikeDatabase()
                     ->GetStrikes(test_scenario.merge_candidate->guid()));
  } else if (test_scenario.merge_candidate.has_value()) {
    // In all other cases, the number of strikes should be unaltered.
    EXPECT_EQ(1, mock_personal_data_manager_.GetProfileUpdateStrikeDatabase()
                     ->GetStrikes(test_scenario.merge_candidate->guid()));
  }
}

// Test that a profile is correctly imported when no other profile is stored
// yet.
TEST_F(AddressProfileSaveManagerTest, SaveNewProfile) {
  AutofillProfile observed_profile = test::StandardProfile();

  ImportScenarioTestCase test_scenario{
      .existing_profiles = {},
      .observed_profile = observed_profile,
      .is_prompt_expected = true,
      .user_decision = UserDecision::kAccepted,
      .expected_import_type = AutofillProfileImportType::kNewProfile,
      .is_profile_change_expected = true,
      .merge_candidate = absl::nullopt,
      .import_candidate = observed_profile,
      .expected_final_profiles = {observed_profile}};

  TestImportScenario(test_scenario);
}

// Test that a profile is correctly imported when no other profile is stored
// yet but another profile is added while waiting for the user response.
TEST_F(AddressProfileSaveManagerTest, SaveNewProfile_ProfileAddedWhileWaiting) {
  AutofillProfile observed_profile = test::StandardProfile();
  AutofillProfile profile_added_while_waiting =
      test::DifferentFromStandardProfile();

  ImportScenarioTestCase test_scenario{
      .existing_profiles = {},
      .observed_profile = observed_profile,
      .is_prompt_expected = true,
      .user_decision = UserDecision::kAccepted,
      .expected_import_type = AutofillProfileImportType::kNewProfile,
      .is_profile_change_expected = true,
      .merge_candidate = absl::nullopt,
      .import_candidate = observed_profile,
      .expected_final_profiles = {observed_profile,
                                  profile_added_while_waiting},
      .profile_to_be_added_while_waiting = profile_added_while_waiting};

  TestImportScenario(test_scenario);
}

// Test that a profile is not imported and that the user is not prompted if the
// domain is blocked for imorting new profiles.
TEST_F(AddressProfileSaveManagerTest, SaveNewProfileOnBlockedDomain) {
  AutofillProfile observed_profile = test::StandardProfile();

  ImportScenarioTestCase test_scenario{
      .existing_profiles = {},
      .observed_profile = observed_profile,
      .is_prompt_expected = false,
      .user_decision = UserDecision::kUserNotAsked,
      .expected_import_type = AutofillProfileImportType::kSuppressedNewProfile,
      .is_profile_change_expected = false,
      .merge_candidate = absl::nullopt,
      .import_candidate = absl::nullopt,
      .expected_final_profiles = {},
      .new_profiles_suppresssed_for_domain = true};

  TestImportScenario(test_scenario);
}

// Test that a profile is correctly imported when no other profile is stored
// yet. Here, `kUserNotAsked` is supplied which is done as a fallback in case
// the UI is unavailable for technical reasons.
TEST_F(AddressProfileSaveManagerTest, SaveNewProfile_UserNotAskedFallback) {
  AutofillProfile observed_profile = test::StandardProfile();

  ImportScenarioTestCase test_scenario{
      .existing_profiles = {},
      .observed_profile = observed_profile,
      .is_prompt_expected = true,
      .user_decision = UserDecision::kUserNotAsked,
      .expected_import_type = AutofillProfileImportType::kNewProfile,
      .is_profile_change_expected = true,
      .merge_candidate = absl::nullopt,
      .import_candidate = observed_profile,
      .expected_final_profiles = {observed_profile}};

  TestImportScenario(test_scenario);
}

// Test that a profile is correctly imported when no other profile is stored
// yet. Here, the profile is edited by the user.
TEST_F(AddressProfileSaveManagerTest, SaveNewProfile_Edited) {
  AutofillProfile observed_profile = test::StandardProfile();
  AutofillProfile edited_profile = test::DifferentFromStandardProfile();
  // The edited profile must have the same GUID then the observed one.
  test::CopyGUID(observed_profile, &edited_profile);

  ImportScenarioTestCase test_scenario{
      .existing_profiles = {},
      .observed_profile = observed_profile,
      .is_prompt_expected = true,
      .user_decision = UserDecision::kEditAccepted,
      .edited_profile = edited_profile,
      .expected_import_type = AutofillProfileImportType::kNewProfile,
      .is_profile_change_expected = true,
      .merge_candidate = absl::nullopt,
      .import_candidate = observed_profile,
      .expected_final_profiles = {edited_profile},
      .expected_edited_types_for_metrics = {
          AutofillMetrics::SettingsVisibleFieldTypeForMetrics::kName,
          AutofillMetrics::SettingsVisibleFieldTypeForMetrics::kStreetAddress,
          AutofillMetrics::SettingsVisibleFieldTypeForMetrics::kCity,
          AutofillMetrics::SettingsVisibleFieldTypeForMetrics::kZip}};

  TestImportScenario(test_scenario);
}

// Test that a decline to import a new profile is handled correctly.
TEST_F(AddressProfileSaveManagerTest, SaveNewProfile_Declined) {
  AutofillProfile observed_profile = test::StandardProfile();

  ImportScenarioTestCase test_scenario{
      .existing_profiles = {},
      .observed_profile = observed_profile,
      .is_prompt_expected = true,
      .user_decision = UserDecision::kDeclined,
      .expected_import_type = AutofillProfileImportType::kNewProfile,
      .is_profile_change_expected = false,
      .merge_candidate = absl::nullopt,
      .import_candidate = observed_profile,
      .expected_final_profiles = {}};

  TestImportScenario(test_scenario);
}

// Test that a decline to import a new profile in the message UI is handled
// correctly.
TEST_F(AddressProfileSaveManagerTest, SaveNewProfile_MessageDeclined) {
  AutofillProfile observed_profile = test::StandardProfile();

  ImportScenarioTestCase test_scenario{
      .existing_profiles = {},
      .observed_profile = observed_profile,
      .is_prompt_expected = true,
      .user_decision = UserDecision::kMessageDeclined,
      .expected_import_type = AutofillProfileImportType::kNewProfile,
      .is_profile_change_expected = false,
      .merge_candidate = absl::nullopt,
      .import_candidate = observed_profile,
      .expected_final_profiles = {}};

  TestImportScenario(test_scenario);
}

// Test that the observation of a duplicate profile has no effect.
TEST_F(AddressProfileSaveManagerTest, ImportDuplicateProfile) {
  // Note that the profile is created twice to enforce different GUIDs.
  AutofillProfile existing_profile = test::StandardProfile();
  AutofillProfile observed_profile = test::StandardProfile();

  ImportScenarioTestCase test_scenario{
      .existing_profiles = {existing_profile},
      .observed_profile = observed_profile,
      .is_prompt_expected = false,
      .user_decision = UserDecision::kAccepted,
      .expected_import_type = AutofillProfileImportType::kDuplicateImport,
      .is_profile_change_expected = false,
      .merge_candidate = absl::nullopt,
      .import_candidate = absl::nullopt,
      .expected_final_profiles = {existing_profile}};

  TestImportScenario(test_scenario);
}

// Test that the observation of quasi identical profile that has a different
// structure in the name will result in a silent update.
TEST_F(AddressProfileSaveManagerTest, SilentlyUpdateProfile) {
  AutofillProfile observed_profile = test::StandardProfile();
  AutofillProfile updateable_profile = test::UpdateableStandardProfile();
  AutofillProfile final_profile = observed_profile;
  test::CopyGUID(updateable_profile, &final_profile);

  ImportScenarioTestCase test_scenario{
      .existing_profiles = {updateable_profile},
      .observed_profile = observed_profile,
      .is_prompt_expected = false,
      .expected_import_type = AutofillProfileImportType::kSilentUpdate,
      .is_profile_change_expected = true,
      .merge_candidate = absl::nullopt,
      .import_candidate = absl::nullopt,
      .expected_final_profiles = {final_profile}};
  TestImportScenario(test_scenario);
}

// Test that the observation of quasi identical profile that has a different
// structure in the name will result in a silent update even though the domain
// is blocked for new profile imports.
TEST_F(AddressProfileSaveManagerTest, SilentlyUpdateProfileOnBlockedDomain) {
  AutofillProfile observed_profile = test::StandardProfile();
  AutofillProfile updateable_profile = test::UpdateableStandardProfile();
  AutofillProfile final_profile = observed_profile;
  test::CopyGUID(updateable_profile, &final_profile);

  ImportScenarioTestCase test_scenario{
      .existing_profiles = {updateable_profile},
      .observed_profile = observed_profile,
      .is_prompt_expected = false,
      .expected_import_type = AutofillProfileImportType::kSilentUpdate,
      .is_profile_change_expected = true,
      .merge_candidate = absl::nullopt,
      .import_candidate = absl::nullopt,
      .expected_final_profiles = {final_profile},
      .new_profiles_suppresssed_for_domain = true};
  TestImportScenario(test_scenario);
}

// Test that the observation of quasi identical profile that has a different
// structure in the name will result in a silent update even when the profile
// has the legacy property of being verified.
TEST_F(AddressProfileSaveManagerTest, SilentlyUpdateVerifiedProfile) {
  AutofillProfile observed_profile = test::StandardProfile();

  AutofillProfile updateable_profile = test::UpdateableStandardProfile();
  updateable_profile.set_origin(kSettingsOrigin);
  ASSERT_TRUE(updateable_profile.IsVerified());

  AutofillProfile final_profile = observed_profile;
  test::CopyGUID(updateable_profile, &final_profile);

  ImportScenarioTestCase test_scenario{
      .existing_profiles = {updateable_profile},
      .observed_profile = observed_profile,
      .is_prompt_expected = false,
      .expected_import_type = AutofillProfileImportType::kSilentUpdate,
      .is_profile_change_expected = true,
      .merge_candidate = absl::nullopt,
      .import_candidate = absl::nullopt,
      .expected_final_profiles = {final_profile}};
  TestImportScenario(test_scenario);
}

// Test the observation of a profile that can only be merged with a
// settings-visible change. Here, `kUserNotAsked` is returned as the fallback
// mechanism when the UI is not available for technical reasons.
TEST_F(AddressProfileSaveManagerTest,
       UserConfirmableMerge_UserNotAskedFallback) {
  AutofillProfile observed_profile = test::StandardProfile();
  AutofillProfile mergeable_profile = test::SubsetOfStandardProfile();
  AutofillProfile final_profile = observed_profile;
  test::CopyGUID(mergeable_profile, &final_profile);

  ImportScenarioTestCase test_scenario{
      .existing_profiles = {mergeable_profile},
      .observed_profile = observed_profile,
      .is_prompt_expected = true,
      .user_decision = UserDecision::kUserNotAsked,
      .expected_import_type = AutofillProfileImportType::kConfirmableMerge,
      .is_profile_change_expected = true,
      .merge_candidate = mergeable_profile,
      .import_candidate = final_profile,
      .expected_final_profiles = {final_profile}};

  TestImportScenario(test_scenario);
}

// Test the observation of a profile that can only be merged with a
// settings-visible change.
TEST_F(AddressProfileSaveManagerTest, UserConfirmableMerge) {
  AutofillProfile observed_profile = test::StandardProfile();
  AutofillProfile mergeable_profile = test::SubsetOfStandardProfile();
  AutofillProfile final_profile = observed_profile;
  test::CopyGUID(mergeable_profile, &final_profile);

  ImportScenarioTestCase test_scenario{
      .existing_profiles = {mergeable_profile},
      .observed_profile = observed_profile,
      .is_prompt_expected = true,
      .user_decision = UserDecision::kAccepted,
      .expected_import_type = AutofillProfileImportType::kConfirmableMerge,
      .is_profile_change_expected = true,
      .merge_candidate = mergeable_profile,
      .import_candidate = final_profile,
      .expected_final_profiles = {final_profile},
      .expected_affeceted_types_in_merge_for_metrics = {
          AutofillMetrics::SettingsVisibleFieldTypeForMetrics::kZip,
          AutofillMetrics::SettingsVisibleFieldTypeForMetrics::kCity}};

  TestImportScenario(test_scenario);
}

// Test the observation of a profile that can only be merged with a
// settings-visible change but the mergeable profile is blocked for updates.
TEST_F(AddressProfileSaveManagerTest, UserConfirmableMerge_BlockedProfile) {
  AutofillProfile observed_profile = test::StandardProfile();
  AutofillProfile mergeable_profile = test::SubsetOfStandardProfile();
  AutofillProfile final_profile = observed_profile;
  test::CopyGUID(mergeable_profile, &final_profile);

  ImportScenarioTestCase test_scenario{
      .existing_profiles = {mergeable_profile},
      .observed_profile = observed_profile,
      .is_prompt_expected = false,
      .user_decision = UserDecision::kUserNotAsked,
      .expected_import_type =
          AutofillProfileImportType::kSuppressedConfirmableMerge,
      .is_profile_change_expected = false,
      .expected_final_profiles = {mergeable_profile},
      .blocked_guids_for_updates = {mergeable_profile.guid()}};

  TestImportScenario(test_scenario);
}

// Test the observation of a profile that can only be merged with a
// settings-visible change. The existing profile has the legacy property of
// being verified.
TEST_F(AddressProfileSaveManagerTest, UserConfirmableMerge_VerifiedProfile) {
  AutofillProfile observed_profile = test::StandardProfile();

  AutofillProfile mergeable_profile = test::SubsetOfStandardProfile();
  mergeable_profile.set_origin(kSettingsOrigin);
  ASSERT_TRUE(mergeable_profile.IsVerified());

  AutofillProfile final_profile = observed_profile;
  test::CopyGUID(mergeable_profile, &final_profile);

  ImportScenarioTestCase test_scenario{
      .existing_profiles = {mergeable_profile},
      .observed_profile = observed_profile,
      .is_prompt_expected = true,
      .user_decision = UserDecision::kAccepted,
      .expected_import_type = AutofillProfileImportType::kConfirmableMerge,
      .is_profile_change_expected = true,
      .merge_candidate = mergeable_profile,
      .import_candidate = final_profile,
      .expected_final_profiles = {final_profile},
      .expected_affeceted_types_in_merge_for_metrics = {
          AutofillMetrics::SettingsVisibleFieldTypeForMetrics::kZip,
          AutofillMetrics::SettingsVisibleFieldTypeForMetrics::kCity}};

  TestImportScenario(test_scenario);
}

// Test the observation of a profile that can only be merged with a
// settings-visible change.
TEST_F(AddressProfileSaveManagerTest, UserConfirmableMerge_Edited) {
  AutofillProfile observed_profile = test::StandardProfile();
  AutofillProfile mergeable_profile = test::SubsetOfStandardProfile();
  AutofillProfile edited_profile = test::DifferentFromStandardProfile();

  AutofillProfile import_candidate = observed_profile;
  test::CopyGUID(mergeable_profile, &import_candidate);
  test::CopyGUID(mergeable_profile, &edited_profile);

  ImportScenarioTestCase test_scenario{
      .existing_profiles = {mergeable_profile},
      .observed_profile = observed_profile,
      .is_prompt_expected = true,
      .user_decision = UserDecision::kEditAccepted,
      .edited_profile = edited_profile,
      .expected_import_type = AutofillProfileImportType::kConfirmableMerge,
      .is_profile_change_expected = true,
      .merge_candidate = mergeable_profile,
      .import_candidate = import_candidate,
      .expected_final_profiles = {edited_profile},
      .expected_edited_types_for_metrics = {
          AutofillMetrics::SettingsVisibleFieldTypeForMetrics::kName,
          AutofillMetrics::SettingsVisibleFieldTypeForMetrics::kStreetAddress,
          AutofillMetrics::SettingsVisibleFieldTypeForMetrics::kCity,
          AutofillMetrics::SettingsVisibleFieldTypeForMetrics::kZip}};

  TestImportScenario(test_scenario);
}

// Test the observation of a profile that can only be merged with a
// settings-visible change but the import is declined by the user.
TEST_F(AddressProfileSaveManagerTest, UserConfirmableMerge_Declined) {
  AutofillProfile observed_profile = test::StandardProfile();
  AutofillProfile mergeable_profile = test::SubsetOfStandardProfile();
  AutofillProfile final_profile = observed_profile;
  test::CopyGUID(mergeable_profile, &final_profile);

  ImportScenarioTestCase test_scenario{
      .existing_profiles = {mergeable_profile},
      .observed_profile = observed_profile,
      .is_prompt_expected = true,
      .user_decision = UserDecision::kDeclined,
      .expected_import_type = AutofillProfileImportType::kConfirmableMerge,
      .is_profile_change_expected = false,
      .merge_candidate = mergeable_profile,
      .import_candidate = final_profile,
      .expected_final_profiles = {mergeable_profile}};

  TestImportScenario(test_scenario);
}

// Test a mixed scenario in which a duplicate profile already exists, but a
// another profile is mergeable with the observed profile.
TEST_F(AddressProfileSaveManagerTest, UserConfirmableMergeAndDuplicate) {
  AutofillProfile observed_profile = test::StandardProfile();
  AutofillProfile existing_duplicate = test::StandardProfile();
  AutofillProfile mergeable_profile = test::SubsetOfStandardProfile();

  AutofillProfile merged_profile = observed_profile;
  test::CopyGUID(mergeable_profile, &merged_profile);

  ImportScenarioTestCase test_scenario{
      .existing_profiles = {existing_duplicate, mergeable_profile},
      .observed_profile = observed_profile,
      .is_prompt_expected = true,
      .user_decision = UserDecision::kAccepted,
      .expected_import_type = AutofillProfileImportType::kConfirmableMerge,
      .is_profile_change_expected = true,
      .merge_candidate = mergeable_profile,
      .import_candidate = merged_profile,
      .expected_final_profiles = {existing_duplicate, merged_profile},
      .expected_affeceted_types_in_merge_for_metrics = {
          AutofillMetrics::SettingsVisibleFieldTypeForMetrics::kZip,
          AutofillMetrics::SettingsVisibleFieldTypeForMetrics::kCity}};

  TestImportScenario(test_scenario);
}

// Test a mixed scenario in which a duplicate profile already exists, but a
// another profile is mergeable with the observed profile. The result should not
// be affected by the fact that the domain is blocked for the import of new
// profiles.
TEST_F(AddressProfileSaveManagerTest,
       UserConfirmableMergeAndDuplicateOnBlockedDomain) {
  AutofillProfile observed_profile = test::StandardProfile();
  AutofillProfile existing_duplicate = test::StandardProfile();
  AutofillProfile mergeable_profile = test::SubsetOfStandardProfile();

  AutofillProfile merged_profile = observed_profile;
  test::CopyGUID(mergeable_profile, &merged_profile);

  ImportScenarioTestCase test_scenario{
      .existing_profiles = {existing_duplicate, mergeable_profile},
      .observed_profile = observed_profile,
      .is_prompt_expected = true,
      .user_decision = UserDecision::kAccepted,
      .expected_import_type = AutofillProfileImportType::kConfirmableMerge,
      .is_profile_change_expected = true,
      .merge_candidate = mergeable_profile,
      .import_candidate = merged_profile,
      .expected_final_profiles = {existing_duplicate, merged_profile},
      .expected_affeceted_types_in_merge_for_metrics =
          {AutofillMetrics::SettingsVisibleFieldTypeForMetrics::kZip,
           AutofillMetrics::SettingsVisibleFieldTypeForMetrics::kCity},
      .new_profiles_suppresssed_for_domain = true};

  TestImportScenario(test_scenario);
}

// Test a mixed scenario in which a duplicate profile already exists, but a
// another profile is mergeable with the observed profile and yet another
// profile can be silently updated.
TEST_F(AddressProfileSaveManagerTest,
       UserConfirmableMergeAndUpdateAndDuplicate) {
  AutofillProfile observed_profile = test::StandardProfile();
  AutofillProfile existing_duplicate = test::StandardProfile();
  AutofillProfile updateable_profile = test::UpdateableStandardProfile();
  AutofillProfile mergeable_profile = test::SubsetOfStandardProfile();

  // Both the mergeable and updateable profile should have the same values as
  // the observed profile.
  AutofillProfile merged_profile = observed_profile;
  AutofillProfile updated_profile = observed_profile;
  // However, the GUIDs must be maintained.
  test::CopyGUID(updateable_profile, &updated_profile);
  test::CopyGUID(mergeable_profile, &merged_profile);

  ImportScenarioTestCase test_scenario{
      .existing_profiles = {existing_duplicate, mergeable_profile,
                            updateable_profile},
      .observed_profile = observed_profile,
      .is_prompt_expected = true,
      .user_decision = UserDecision::kAccepted,
      .expected_import_type =
          AutofillProfileImportType::kConfirmableMergeAndSilentUpdate,
      .is_profile_change_expected = true,
      .merge_candidate = mergeable_profile,
      .import_candidate = merged_profile,
      .expected_final_profiles = {existing_duplicate, updated_profile,
                                  merged_profile},
      .expected_affeceted_types_in_merge_for_metrics = {
          AutofillMetrics::SettingsVisibleFieldTypeForMetrics::kZip,
          AutofillMetrics::SettingsVisibleFieldTypeForMetrics::kCity}};

  TestImportScenario(test_scenario);
}

// Same as above, but the merge candidate is blocked for updates.
TEST_F(AddressProfileSaveManagerTest,
       UserConfirmableMergeAndUpdateAndDuplicate_Blocked) {
  AutofillProfile observed_profile = test::StandardProfile();
  AutofillProfile existing_duplicate = test::StandardProfile();
  AutofillProfile updateable_profile = test::UpdateableStandardProfile();
  AutofillProfile mergeable_profile = test::SubsetOfStandardProfile();

  // Both the mergeable and updateable profile should have the same values as
  // the observed profile.
  AutofillProfile merged_profile = observed_profile;
  AutofillProfile updated_profile = observed_profile;
  // However, the GUIDs must be maintained.
  test::CopyGUID(updateable_profile, &updated_profile);
  test::CopyGUID(mergeable_profile, &merged_profile);

  ImportScenarioTestCase test_scenario{
      .existing_profiles = {existing_duplicate, mergeable_profile,
                            updateable_profile},
      .observed_profile = observed_profile,
      .is_prompt_expected = false,
      .user_decision = UserDecision::kUserNotAsked,
      .expected_import_type =
          AutofillProfileImportType::kSuppressedConfirmableMergeAndSilentUpdate,
      .is_profile_change_expected = true,
      .expected_final_profiles = {existing_duplicate, mergeable_profile,
                                  updated_profile},
      .blocked_guids_for_updates = {mergeable_profile.guid()}};

  TestImportScenario(test_scenario);
}

// Test a mixed scenario in which a duplicate profile already exists, but a
// another profile is mergeable with the observed profile and yet another
// profile can be silently updated. Here, the merge is declined.
TEST_F(AddressProfileSaveManagerTest,
       UserConfirmableMergeAndUpdateAndDuplicate_Declined) {
  AutofillProfile observed_profile = test::StandardProfile();
  AutofillProfile existing_duplicate = test::StandardProfile();
  AutofillProfile updateable_profile = test::UpdateableStandardProfile();
  AutofillProfile mergeable_profile = test::SubsetOfStandardProfile();

  // Both the mergeable and updateable profile should have the same values as
  // the observed profile.
  AutofillProfile merged_profile = observed_profile;
  AutofillProfile updated_profile = observed_profile;
  // However, the GUIDs must be maintained.
  test::CopyGUID(updateable_profile, &updated_profile);
  test::CopyGUID(mergeable_profile, &merged_profile);

  ImportScenarioTestCase test_scenario{
      .existing_profiles = {existing_duplicate, mergeable_profile,
                            updateable_profile},
      .observed_profile = observed_profile,
      .is_prompt_expected = true,
      .user_decision = UserDecision::kDeclined,
      .expected_import_type =
          AutofillProfileImportType::kConfirmableMergeAndSilentUpdate,
      .is_profile_change_expected = true,
      .merge_candidate = mergeable_profile,
      .import_candidate = merged_profile,
      .expected_final_profiles = {existing_duplicate, updated_profile,
                                  mergeable_profile}};

  TestImportScenario(test_scenario);
}

// Test a mixed scenario in which a duplicate profile already exists, but a
// another profile is mergeable with the observed profile and yet another
// profile can be silently updated. Here, the merge is accepted with edits.
TEST_F(AddressProfileSaveManagerTest,
       UserConfirmableMergeAndUpdateAndDuplicate_Edited) {
  AutofillProfile observed_profile = test::StandardProfile();
  AutofillProfile existing_duplicate = test::StandardProfile();
  AutofillProfile updateable_profile = test::UpdateableStandardProfile();
  AutofillProfile mergeable_profile = test::SubsetOfStandardProfile();
  AutofillProfile edited_profile = test::DifferentFromStandardProfile();

  // Both the mergeable and updateable profile should have the same values as
  // the observed profile.
  AutofillProfile merged_profile = observed_profile;
  AutofillProfile updated_profile = observed_profile;
  // However, the GUIDs must be maintained.
  test::CopyGUID(updateable_profile, &updated_profile);
  test::CopyGUID(mergeable_profile, &merged_profile);
  test::CopyGUID(mergeable_profile, &edited_profile);

  ImportScenarioTestCase test_scenario{
      .existing_profiles = {existing_duplicate, mergeable_profile,
                            updateable_profile},
      .observed_profile = observed_profile,
      .is_prompt_expected = true,
      .user_decision = UserDecision::kEditAccepted,
      .edited_profile = edited_profile,
      .expected_import_type =
          AutofillProfileImportType::kConfirmableMergeAndSilentUpdate,
      .is_profile_change_expected = true,
      .merge_candidate = mergeable_profile,
      .import_candidate = merged_profile,
      .expected_final_profiles = {existing_duplicate, updated_profile,
                                  edited_profile},
      .expected_edited_types_for_metrics = {
          AutofillMetrics::SettingsVisibleFieldTypeForMetrics::kName,
          AutofillMetrics::SettingsVisibleFieldTypeForMetrics::kStreetAddress,
          AutofillMetrics::SettingsVisibleFieldTypeForMetrics::kCity,
          AutofillMetrics::SettingsVisibleFieldTypeForMetrics::kZip}};

  TestImportScenario(test_scenario);
}

TEST_F(AddressProfileSaveManagerTest, SaveProfileWhenNoSavePrompt) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillAddressProfileSavePrompt);

  AddressProfileSaveManager save_manager(&autofill_client_,
                                         &mock_personal_data_manager_);
  AutofillProfile test_profile = test::GetFullProfile();
  EXPECT_CALL(mock_personal_data_manager_, SaveImportedProfile(test_profile));
  save_manager.ImportProfileFromForm(test_profile, "en_US",
                                     GURL("https://www.noprompt.com"),
                                     /*allow_only_silent_updates=*/false);
}

// Tests that a new profile is not imported when only silent updates are
// allowed.
TEST_F(AddressProfileSaveManagerTest, SilentlyUpdateProfile_SaveNewProfile) {
  AutofillProfile observed_profile = test::StandardProfile();

  ImportScenarioTestCase test_scenario{
      .existing_profiles = {},
      .observed_profile = observed_profile,
      .is_prompt_expected = false,
      .user_decision = UserDecision::kUserNotAsked,
      .expected_import_type =
          AutofillProfileImportType::kUnusableIncompleteProfile,
      .is_profile_change_expected = false,
      .expected_final_profiles = {},
      .allow_only_silent_updates = true};

  TestImportScenario(test_scenario);
}

// Test that the observation of quasi identical profile that has a different
// structure in the name will result in a silent update when only silent updates
// are allowed.
TEST_F(AddressProfileSaveManagerTest,
       SilentlyUpdateProfile_WithIncompleteProfile) {
  AutofillProfile observed_profile = test::StandardProfile();
  AutofillProfile updateable_profile = test::UpdateableStandardProfile();
  AutofillProfile final_profile = observed_profile;
  test::CopyGUID(updateable_profile, &final_profile);

  ImportScenarioTestCase test_scenario{
      .existing_profiles = {updateable_profile},
      .observed_profile = observed_profile,
      .is_prompt_expected = false,
      .expected_import_type =
          AutofillProfileImportType::kSilentUpdateForIncompleteProfile,
      .is_profile_change_expected = true,
      .expected_final_profiles = {final_profile},
      .allow_only_silent_updates = true};
  TestImportScenario(test_scenario);
}

// Tests that the profile's structured name information is silently updated when
// an updated profile is observed with no settings visible difference.
// Silent Update is enabled for the test.
TEST_F(AddressProfileSaveManagerTest,
       SilentlyUpdateProfile_UpdateStructuredName) {
  AutofillProfile updateable_profile;
  test::SetProfileTestValues(
      &updateable_profile,
      {{NAME_FULL, "AAA BBB CCC", VerificationStatus::kObserved},
       {NAME_FIRST, "AAA", VerificationStatus::kParsed},
       {NAME_MIDDLE, "BBB", VerificationStatus::kParsed},
       {NAME_LAST, "CCC", VerificationStatus::kParsed},
       {ADDRESS_HOME_STREET_ADDRESS, "119 Some Avenue",
        VerificationStatus::kObserved},
       {ADDRESS_HOME_COUNTRY, "US", VerificationStatus::kObserved},
       {ADDRESS_HOME_STATE, "CA", VerificationStatus::kObserved},
       {ADDRESS_HOME_ZIP, "99666", VerificationStatus::kObserved},
       {ADDRESS_HOME_CITY, "Los Angeles", VerificationStatus::kObserved}});

  AutofillProfile observed_profile;
  test::SetProfileTestValues(
      &observed_profile,
      {{NAME_FULL, "AAA BBB CCC", VerificationStatus::kObserved},
       {NAME_FIRST, "AAA", VerificationStatus::kParsed},
       {NAME_MIDDLE, "", VerificationStatus::kParsed},
       {NAME_LAST, "BBB CCC", VerificationStatus::kParsed},
       {ADDRESS_HOME_STREET_ADDRESS, "119 Some Avenue",
        VerificationStatus::kObserved},
       {ADDRESS_HOME_COUNTRY, "US", VerificationStatus::kObserved},
       {ADDRESS_HOME_STATE, "CA", VerificationStatus::kObserved},
       {ADDRESS_HOME_ZIP, "99666", VerificationStatus::kObserved},
       {ADDRESS_HOME_CITY, "Los Angeles", VerificationStatus::kObserved}});

  AutofillProfile final_profile = observed_profile;
  test::CopyGUID(updateable_profile, &final_profile);

  ImportScenarioTestCase test_scenario{
      .existing_profiles = {updateable_profile},
      .observed_profile = observed_profile,
      .is_prompt_expected = false,
      .expected_import_type =
          AutofillProfileImportType::kSilentUpdateForIncompleteProfile,
      .is_profile_change_expected = true,
      .expected_final_profiles = {final_profile},
      .allow_only_silent_updates = true};
  TestImportScenario(test_scenario);
}

// Tests that the profile's structured name information is silently updated when
// an updated profile with no address data is observed with no settings visible
// difference.
// Silent Update is enabled for the test.
TEST_F(AddressProfileSaveManagerTest,
       SilentlyUpdateProfile_UpdateStructuredNameWithIncompleteProfile) {
  AutofillProfile updateable_profile;
  test::SetProfileTestValues(
      &updateable_profile,
      {{NAME_FULL, "AAA BBB CCC", VerificationStatus::kObserved},
       {NAME_FIRST, "AAA", VerificationStatus::kParsed},
       {NAME_MIDDLE, "BBB", VerificationStatus::kParsed},
       {NAME_LAST, "CCC", VerificationStatus::kParsed},
       {ADDRESS_HOME_STREET_ADDRESS, "119 Some Avenue",
        VerificationStatus::kObserved},
       {ADDRESS_HOME_COUNTRY, "US", VerificationStatus::kObserved},
       {ADDRESS_HOME_STATE, "CA", VerificationStatus::kObserved},
       {ADDRESS_HOME_ZIP, "99666", VerificationStatus::kObserved},
       {ADDRESS_HOME_CITY, "Los Angeles", VerificationStatus::kObserved}});

  AutofillProfile observed_profile;
  test::SetProfileTestValues(
      &observed_profile,
      {{NAME_FULL, "AAA BBB CCC", VerificationStatus::kObserved},
       {NAME_FIRST, "AAA", VerificationStatus::kParsed},
       {NAME_MIDDLE, "", VerificationStatus::kParsed},
       {NAME_LAST, "BBB CCC", VerificationStatus::kParsed}});

  AutofillProfile final_profile;
  test::SetProfileTestValues(
      &final_profile,
      {{NAME_FULL, "AAA BBB CCC", VerificationStatus::kObserved},
       {NAME_FIRST, "AAA", VerificationStatus::kParsed},
       {NAME_MIDDLE, "", VerificationStatus::kParsed},
       {NAME_LAST, "BBB CCC", VerificationStatus::kParsed},
       {ADDRESS_HOME_STREET_ADDRESS, "119 Some Avenue",
        VerificationStatus::kObserved},
       {ADDRESS_HOME_COUNTRY, "US", VerificationStatus::kObserved},
       {ADDRESS_HOME_STATE, "CA", VerificationStatus::kObserved},
       {ADDRESS_HOME_ZIP, "99666", VerificationStatus::kObserved},
       {ADDRESS_HOME_CITY, "Los Angeles", VerificationStatus::kObserved}});
  test::CopyGUID(updateable_profile, &final_profile);

  ImportScenarioTestCase test_scenario{
      .existing_profiles = {updateable_profile},
      .observed_profile = observed_profile,
      .is_prompt_expected = false,
      .expected_import_type =
          AutofillProfileImportType::kSilentUpdateForIncompleteProfile,
      .is_profile_change_expected = true,
      .expected_final_profiles = {final_profile},
      .allow_only_silent_updates = true};
  TestImportScenario(test_scenario);
}

// Test that the observation of quasi identical profile that has a different
// structure in the name will result in a silent update even though the domain
// is blocked for new profile imports when silent updates are enforced.
TEST_F(AddressProfileSaveManagerTest, SilentlyUpdateProfile_OnBlockedDomain) {
  AutofillProfile observed_profile = test::StandardProfile();
  AutofillProfile updateable_profile = test::UpdateableStandardProfile();
  AutofillProfile final_profile = observed_profile;
  test::CopyGUID(updateable_profile, &final_profile);

  ImportScenarioTestCase test_scenario{
      .existing_profiles = {updateable_profile},
      .observed_profile = observed_profile,
      .is_prompt_expected = false,
      .expected_import_type =
          AutofillProfileImportType::kSilentUpdateForIncompleteProfile,
      .is_profile_change_expected = true,
      .expected_final_profiles = {final_profile},
      .new_profiles_suppresssed_for_domain = true,
      .allow_only_silent_updates = true};
  TestImportScenario(test_scenario);
}

// Test a mixed scenario in which a duplicate profile already exists, but
// another profile is mergeable with the observed profile and yet another
// profile can be silently updated. Only silent updates are allowed for this
// test.
TEST_F(AddressProfileSaveManagerTest,
       SilentlyUpdateProfile_UserConfirmableMergeAndUpdateAndDuplicate) {
  AutofillProfile observed_profile = test::StandardProfile();
  AutofillProfile existing_duplicate = test::StandardProfile();
  AutofillProfile updateable_profile = test::UpdateableStandardProfile();
  AutofillProfile mergeable_profile = test::SubsetOfStandardProfile();

  // The updateable profile should have the same value as the observed profile.
  AutofillProfile updated_profile = observed_profile;
  // However, the GUIDs must be maintained.
  test::CopyGUID(updateable_profile, &updated_profile);

  ImportScenarioTestCase test_scenario{
      .existing_profiles = {existing_duplicate, mergeable_profile,
                            updateable_profile},
      .observed_profile = observed_profile,
      .is_prompt_expected = false,
      .expected_import_type =
          AutofillProfileImportType::kSilentUpdateForIncompleteProfile,
      .is_profile_change_expected = true,
      .expected_final_profiles = {existing_duplicate, mergeable_profile,
                                  updated_profile},
      .allow_only_silent_updates = true};

  TestImportScenario(test_scenario);
}

// Tests that the mergeable profiles are not merged when only silent updates
// are allowed.
TEST_F(AddressProfileSaveManagerTest,
       SilentlyUpdateProfile_UserConfirmableMergeNotAllowed) {
  AutofillProfile observed_profile = test::StandardProfile();
  AutofillProfile mergeable_profile = test::SubsetOfStandardProfile();

  ImportScenarioTestCase test_scenario{
      .existing_profiles = {mergeable_profile},
      .observed_profile = observed_profile,
      .is_prompt_expected = false,
      .user_decision = UserDecision::kUserNotAsked,
      .expected_import_type =
          AutofillProfileImportType::kUnusableIncompleteProfile,
      .is_profile_change_expected = true,
      .expected_final_profiles = {mergeable_profile},
      .allow_only_silent_updates = true};

  TestImportScenario(test_scenario);
}

}  // namespace

}  // namespace autofill
