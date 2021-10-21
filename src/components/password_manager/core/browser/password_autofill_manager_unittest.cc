// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_autofill_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion_test_helpers.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/device_reauth/biometric_authenticator.h"
#include "components/device_reauth/mock_biometric_authenticator.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/password_manager/core/browser/mock_password_feature_manager.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/security_state/core/security_state.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/strings/grit/components_strings.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

// The name of the username/password element in the form.
const char16_t kUsernameName[] = u"username";
const char16_t kInvalidUsername[] = u"no-username";
const char16_t kPasswordName[] = u"password";

const char16_t kAliceUsername[] = u"alice";
const char16_t kAlicePassword[] = u"password";
const char16_t kAliceAccountStoredPassword[] = u"account-stored-password";

using autofill::PopupType;
using autofill::Suggestion;
using autofill::SuggestionVectorIconsAre;
using autofill::SuggestionVectorIdsAre;
using autofill::SuggestionVectorLabelsAre;
using autofill::SuggestionVectorValuesAre;
using autofill::password_generation::PasswordGenerationType;
using base::test::RunOnceCallback;
using device_reauth::BiometricAuthRequester;
using device_reauth::BiometricsAvailability;
using favicon_base::FaviconImageCallback;
using gfx::test::AreImagesEqual;
using testing::_;
using testing::AllOf;
using testing::ElementsAre;
using testing::Eq;
using testing::Field;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::SizeIs;
using testing::Unused;

using UkmEntry = ukm::builders::PageWithPassword;

namespace autofill {
class AutofillPopupDelegate;
}

namespace password_manager {

namespace {

using ReauthSucceeded = PasswordManagerClient::ReauthSucceeded;

constexpr char kMainFrameUrl[] = "https://example.com/";
constexpr char kDropdownSelectedHistogram[] =
    "PasswordManager.PasswordDropdownItemSelected";
constexpr char kDropdownShownHistogram[] =
    "PasswordManager.PasswordDropdownShown";
constexpr char kCredentialsCountFromAccountStoreAfterUnlockHistogram[] =
    "PasswordManager.CredentialsCountFromAccountStoreAfterUnlock";
const gfx::Image kTestFavicon = gfx::test::CreateImage(16, 16);

class MockWebAuthnCredentialsDelegate : public WebAuthnCredentialsDelegate {
 public:
  MOCK_METHOD(bool, IsWebAuthnAutofillEnabled, (), (const, override));
  MOCK_METHOD(void,
              SelectWebAuthnCredential,
              (std::string backend_id),
              (override));
  MOCK_METHOD(std::vector<autofill::Suggestion>,
              GetWebAuthnSuggestions,
              (),
              (const override));
};

class MockPasswordManagerDriver : public StubPasswordManagerDriver {
 public:
  MOCK_METHOD(void,
              FillSuggestion,
              (const std::u16string&, const std::u16string&),
              (override));
  MOCK_METHOD(void,
              PreviewSuggestion,
              (const std::u16string&, const std::u16string&),
              (override));
  MOCK_METHOD(PasswordManager*, GetPasswordManager, (), (override));
};

class TestPasswordManagerClient : public StubPasswordManagerClient {
 public:
  TestPasswordManagerClient() : main_frame_url_(kMainFrameUrl) {}

  MockPasswordManagerDriver* mock_driver() { return &driver_; }
  const GURL& GetLastCommittedURL() const override { return main_frame_url_; }

  const MockPasswordFeatureManager* GetPasswordFeatureManager() const override {
    return feature_manager_.get();
  }

  MockPasswordFeatureManager* GetPasswordFeatureManager() {
    return feature_manager_.get();
  }

  signin::IdentityManager* GetIdentityManager() override {
    return identity_test_env_.identity_manager();
  }

  signin::IdentityTestEnvironment& identity_test_env() {
    return identity_test_env_;
  }

  void SetAccountStorageOptIn(bool opt_in) {
    ON_CALL(*feature_manager_.get(), ShouldShowAccountStorageOptIn)
        .WillByDefault(Return(!opt_in));
  }

  void SetNeedsReSigninForAccountStorage(bool needs_signin) {
    ON_CALL(*feature_manager_.get(), ShouldShowAccountStorageReSignin)
        .WillByDefault(Return(needs_signin));
  }

  void SetBiometricAuthenticator(
      scoped_refptr<device_reauth::MockBiometricAuthenticator>
          biometric_authenticator) {
    biometric_authenticator_ = std::move(biometric_authenticator);
  }

  scoped_refptr<device_reauth::BiometricAuthenticator>
  GetBiometricAuthenticator() override {
    return biometric_authenticator_;
  }

  MOCK_METHOD(void, GeneratePassword, (PasswordGenerationType), (override));
  MOCK_METHOD(void,
              TriggerReauthForPrimaryAccount,
              (signin_metrics::ReauthAccessPoint,
               base::OnceCallback<void(ReauthSucceeded)>),
              (override));
  MOCK_METHOD(void, TriggerSignIn, (signin_metrics::AccessPoint), (override));
  MOCK_METHOD(favicon::FaviconService*, GetFaviconService, (), (override));
  MOCK_METHOD(void,
              NavigateToManagePasswordsPage,
              (ManagePasswordsReferrer),
              (override));
  MOCK_METHOD(WebAuthnCredentialsDelegate*,
              GetWebAuthnCredentialsDelegate,
              (),
              (override));

 private:
  MockPasswordManagerDriver driver_;
  scoped_refptr<device_reauth::MockBiometricAuthenticator>
      biometric_authenticator_ = nullptr;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<MockPasswordFeatureManager> feature_manager_{
      new NiceMock<MockPasswordFeatureManager>};
  GURL main_frame_url_;
};

class MockAutofillClient : public autofill::TestAutofillClient {
 public:
  MockAutofillClient() = default;
  MOCK_METHOD(void,
              ShowAutofillPopup,
              (const autofill::AutofillClient::PopupOpenArgs& open_args,
               base::WeakPtr<autofill::AutofillPopupDelegate> delegate),
              (override));
  MOCK_METHOD(void, PinPopupView, (), (override));
  MOCK_METHOD(PopupOpenArgs, GetReopenPopupArgs, (), (const, override));
  MOCK_METHOD(base::span<const autofill::Suggestion>,
              GetPopupSuggestions,
              (),
              (const, override));
  MOCK_METHOD(void,
              UpdatePopup,
              (const std::vector<autofill::Suggestion>&, PopupType),
              (override));
  MOCK_METHOD(void,
              HideAutofillPopup,
              (autofill::PopupHidingReason),
              (override));
  MOCK_METHOD(void, ExecuteCommand, (int), (override));
};

base::CancelableTaskTracker::TaskId
RespondWithTestIcon(Unused, FaviconImageCallback callback, Unused) {
  favicon_base::FaviconImageResult image_result;
  image_result.image = kTestFavicon;
  std::move(callback).Run(image_result);
  return 1;
}

std::vector<autofill::Suggestion> CreateTestSuggestions(
    bool has_opt_in_and_fill,
    bool has_opt_in_and_generate,
    bool has_re_signin) {
  std::vector<Suggestion> suggestions;
  suggestions.emplace_back(
      /*value=*/"User1", /*label=*/"PW1", /*icon=*/"",
      /*frontend_id=*/autofill::POPUP_ITEM_ID_PASSWORD_ENTRY);
  suggestions.emplace_back(
      /*value=*/"Show all pwds", /*label=*/"", /*icon=*/"",
      /*frontend_id=*/autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY);
  if (has_opt_in_and_fill) {
    suggestions.emplace_back(
        /*value=*/"Unlock passwords and fill", /*label=*/"", /*icon=*/"",
        /*frontend_id=*/
        autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN);
  }
  if (has_opt_in_and_generate) {
    suggestions.emplace_back(
        /*value=*/"Unlock passwords and generate", /*label=*/"", /*icon=*/"",
        /*frontend_id=*/
        autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN_AND_GENERATE);
  }
  if (has_re_signin) {
    suggestions.emplace_back(
        /*value=*/"Sign in to access passwords", /*label=*/"", /*icon=*/"",
        /*frontend_id=*/
        autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_RE_SIGNIN);
  }
  return suggestions;
}

std::vector<autofill::Suggestion> SetLoading(
    std::vector<autofill::Suggestion> suggestions,
    int index_of_loading_element) {
  suggestions[index_of_loading_element].is_loading =
      Suggestion::IsLoading(true);
  return suggestions;
}

autofill::AutofillClient::PopupOpenArgs CreateReopenArgsWithTestSuggestions(
    bool has_opt_in_and_fill,
    bool has_opt_in_and_generate,
    bool has_re_signin) {
  return {
      gfx::RectF(), base::i18n::LEFT_TO_RIGHT,
      CreateTestSuggestions(has_opt_in_and_fill, has_opt_in_and_generate,
                            has_re_signin),
      autofill::AutofillClient::PopupOpenArgs::AutoselectFirstSuggestion(false),
      autofill::PopupType::kPasswords};
}

}  // namespace

class PasswordAutofillManagerTest : public testing::Test {
 protected:
  PasswordAutofillManagerTest()
      : test_username_(kAliceUsername), test_password_(kAlicePassword) {}

  void SetUp() override {
    // Add a preferred login and an additional login to the FillData.
    autofill::FormFieldData username_field;
    username_field.name = kUsernameName;
    username_field.value = test_username_;
    fill_data_.username_field = username_field;

    autofill::FormFieldData password_field;
    password_field.name = kPasswordName;
    password_field.value = test_password_;
    fill_data_.password_field = password_field;
  }

  void InitializePasswordAutofillManager(TestPasswordManagerClient* client,
                                         MockAutofillClient* autofill_client) {
    password_autofill_manager_ = std::make_unique<PasswordAutofillManager>(
        client->mock_driver(), autofill_client, client);
    favicon::MockFaviconService favicon_service;
    EXPECT_CALL(*client, GetFaviconService())
        .WillOnce(Return(&favicon_service));
    EXPECT_CALL(favicon_service,
                GetFaviconImageForPageURL(fill_data_.url, _, _));
    password_autofill_manager_->OnAddPasswordFillData(fill_data_);
    testing::Mock::VerifyAndClearExpectations(client);
    // Suppress the warnings in the tests.
    EXPECT_CALL(*client, GetFaviconService()).WillRepeatedly(Return(nullptr));
  }

  autofill::PasswordFormFillData CreateTestFormFillData() {
    autofill::PasswordFormFillData data;
    data.username_field.value = test_username_;
    data.password_field.value = test_password_;
    data.preferred_realm = "http://foo.com/";
    return data;
  }

  std::u16string GetManagePasswordsTitle() {
    return l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_MANAGE_PASSWORDS);
  }

  std::string GetManagePasswordsIcon() {
    // The "Manage passwords" entry only has an icon if
    // kEnablePasswordsAccountStorage is enabled.
    std::string settings_icon;
    if (base::FeatureList::IsEnabled(
            password_manager::features::kEnablePasswordsAccountStorage)) {
      return "settingsIcon";
    }
    return std::string();
  }

 protected:
  autofill::PasswordFormFillData& fill_data() { return fill_data_; }

  std::unique_ptr<PasswordAutofillManager> password_autofill_manager_;

  scoped_refptr<device_reauth::MockBiometricAuthenticator> authenticator_ =
      base::MakeRefCounted<device_reauth::MockBiometricAuthenticator>();

  std::u16string test_username_;
  std::u16string test_password_;

 private:
  autofill::PasswordFormFillData fill_data_;

  // The TestAutofillDriver uses a SequencedWorkerPool which expects the
  // existence of a MessageLoop.
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(PasswordAutofillManagerTest, FillSuggestion) {
  TestPasswordManagerClient client;
  InitializePasswordAutofillManager(&client, nullptr);

  EXPECT_CALL(*client.mock_driver(),
              FillSuggestion(test_username_, test_password_));
  EXPECT_TRUE(
      password_autofill_manager_->FillSuggestionForTest(test_username_));
  testing::Mock::VerifyAndClearExpectations(client.mock_driver());

  EXPECT_CALL(*client.mock_driver(), FillSuggestion(_, _)).Times(0);
  EXPECT_FALSE(
      password_autofill_manager_->FillSuggestionForTest(kInvalidUsername));

  password_autofill_manager_->DidNavigateMainFrame();
  EXPECT_FALSE(
      password_autofill_manager_->FillSuggestionForTest(test_username_));
}

TEST_F(PasswordAutofillManagerTest, PreviewSuggestion) {
  TestPasswordManagerClient client;
  InitializePasswordAutofillManager(&client, nullptr);

  EXPECT_CALL(*client.mock_driver(),
              PreviewSuggestion(test_username_, test_password_));
  EXPECT_TRUE(
      password_autofill_manager_->PreviewSuggestionForTest(test_username_));
  testing::Mock::VerifyAndClearExpectations(client.mock_driver());

  EXPECT_CALL(*client.mock_driver(), PreviewSuggestion(_, _)).Times(0);
  EXPECT_FALSE(
      password_autofill_manager_->PreviewSuggestionForTest(kInvalidUsername));

  password_autofill_manager_->DidNavigateMainFrame();
  EXPECT_FALSE(
      password_autofill_manager_->PreviewSuggestionForTest(test_username_));
}

// Test that the popup is marked as visible after receiving password
// suggestions.
TEST_F(PasswordAutofillManagerTest, ExternalDelegatePasswordSuggestions) {
  for (bool is_suggestion_on_password_field : {false, true}) {
    SCOPED_TRACE(testing::Message() << "is_suggestion_on_password_field = "
                                    << is_suggestion_on_password_field);
    TestPasswordManagerClient client;
    NiceMock<MockAutofillClient> autofill_client;
    InitializePasswordAutofillManager(&client, &autofill_client);

    // Load filling and favicon data.
    autofill::PasswordFormFillData data = CreateTestFormFillData();
    data.uses_account_store = false;
    favicon::MockFaviconService favicon_service;
    EXPECT_CALL(client, GetFaviconService()).WillOnce(Return(&favicon_service));
    EXPECT_CALL(favicon_service, GetFaviconImageForPageURL(data.url, _, _))
        .WillOnce(Invoke(RespondWithTestIcon));
    password_autofill_manager_->OnAddPasswordFillData(data);

    // Show the popup and verify the suggestions.
    autofill::AutofillClient::PopupOpenArgs open_args;
    EXPECT_CALL(autofill_client, ShowAutofillPopup)
        .WillOnce(testing::SaveArg<0>(&open_args));

    int show_suggestion_options =
        is_suggestion_on_password_field ? autofill::IS_PASSWORD_FIELD : 0;
    password_autofill_manager_->OnShowPasswordSuggestions(
        base::i18n::RIGHT_TO_LEFT, std::u16string(), show_suggestion_options,
        gfx::RectF());
    ASSERT_GE(open_args.suggestions.size(), 1u);
    EXPECT_THAT(open_args.suggestions,
                SuggestionVectorIdsAre(ElementsAre(
                    is_suggestion_on_password_field
                        ? autofill::POPUP_ITEM_ID_PASSWORD_ENTRY
                        : autofill::POPUP_ITEM_ID_USERNAME_ENTRY,
                    autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY)));
    EXPECT_TRUE(
        AreImagesEqual(open_args.suggestions[0].custom_icon, kTestFavicon));

    EXPECT_CALL(*client.mock_driver(),
                FillSuggestion(test_username_, test_password_));
    // Accepting a suggestion should trigger a call to hide the popup.
    EXPECT_CALL(
        autofill_client,
        HideAutofillPopup(autofill::PopupHidingReason::kAcceptSuggestion));
    base::HistogramTester histograms;
    password_autofill_manager_->DidAcceptSuggestion(
        test_username_,
        is_suggestion_on_password_field
            ? autofill::POPUP_ITEM_ID_PASSWORD_ENTRY
            : autofill::POPUP_ITEM_ID_USERNAME_ENTRY,
        std::string(), 1);
    histograms.ExpectUniqueSample(
        kDropdownSelectedHistogram,
        metrics_util::PasswordDropdownSelectedOption::kPassword, 1);
  }
}

// Test that suggestions are filled correctly if account-stored and local
// suggestion have duplicates.
TEST_F(PasswordAutofillManagerTest,
       ExternalDelegateAccountStorePasswordSuggestions) {
  for (bool is_suggestion_on_password_field : {false, true}) {
    SCOPED_TRACE(testing::Message() << "is_suggestion_on_password_field = "
                                    << is_suggestion_on_password_field);
    TestPasswordManagerClient client;
    NiceMock<MockAutofillClient> autofill_client;
    InitializePasswordAutofillManager(&client, &autofill_client);

    // Load filling data and account-stored duplicate with a different password.
    autofill::PasswordFormFillData data = CreateTestFormFillData();
    autofill::PasswordAndMetadata duplicate;
    duplicate.password = kAliceAccountStoredPassword;
    duplicate.realm = data.preferred_realm;
    duplicate.uses_account_store = true;
    duplicate.username = data.username_field.value;
    data.additional_logins.push_back(duplicate);
    favicon::MockFaviconService favicon_service;
    EXPECT_CALL(client, GetFaviconService()).WillOnce(Return(&favicon_service));
    EXPECT_CALL(favicon_service, GetFaviconImageForPageURL(data.url, _, _))
        .WillOnce(Invoke(RespondWithTestIcon));
    password_autofill_manager_->OnAddPasswordFillData(data);

    // Show the popup and verify local and account-stored suggestion coexist.
    autofill::AutofillClient::PopupOpenArgs open_args;
    EXPECT_CALL(autofill_client, ShowAutofillPopup)
        .WillOnce(testing::SaveArg<0>(&open_args));
    password_autofill_manager_->OnShowPasswordSuggestions(
        base::i18n::RIGHT_TO_LEFT, std::u16string(),
        is_suggestion_on_password_field
            ? autofill::IS_PASSWORD_FIELD
            : autofill::ShowPasswordSuggestionsOptions(),
        gfx::RectF());
    ASSERT_GE(open_args.suggestions.size(), 2u);
    EXPECT_THAT(
        open_args.suggestions,
        SuggestionVectorIdsAre(ElementsAre(
            is_suggestion_on_password_field
                ? autofill::POPUP_ITEM_ID_PASSWORD_ENTRY
                : autofill::POPUP_ITEM_ID_USERNAME_ENTRY,
            is_suggestion_on_password_field
                ? autofill::POPUP_ITEM_ID_ACCOUNT_STORAGE_PASSWORD_ENTRY
                : autofill::POPUP_ITEM_ID_ACCOUNT_STORAGE_USERNAME_ENTRY,
            autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY)));
    EXPECT_TRUE(
        AreImagesEqual(open_args.suggestions[0].custom_icon, kTestFavicon));
    EXPECT_TRUE(
        AreImagesEqual(open_args.suggestions[1].custom_icon, kTestFavicon));

    // When selecting the account-stored credential, make sure the filled
    // password belongs to the selected credential (and not to the first match).
    EXPECT_CALL(*client.mock_driver(),
                FillSuggestion(test_username_, duplicate.password));
    EXPECT_CALL(
        autofill_client,
        HideAutofillPopup(autofill::PopupHidingReason::kAcceptSuggestion));
    password_autofill_manager_->DidAcceptSuggestion(
        test_username_,
        is_suggestion_on_password_field
            ? autofill::POPUP_ITEM_ID_ACCOUNT_STORAGE_PASSWORD_ENTRY
            : autofill::POPUP_ITEM_ID_ACCOUNT_STORAGE_USERNAME_ENTRY,
        std::string(),
        /*position=*/1);
  }
}

// Test that the popup is updated once account-stored suggestions are unlocked.
TEST_F(PasswordAutofillManagerTest, ShowOptInAndFillButton) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);
  client.SetAccountStorageOptIn(false);

  // Show the popup and verify the suggestions.
  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));
  password_autofill_manager_->OnShowPasswordSuggestions(
      base::i18n::RIGHT_TO_LEFT, std::u16string(),
      autofill::SHOW_ALL | autofill::IS_PASSWORD_FIELD, gfx::RectF());
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(ElementsAre(
                  autofill::POPUP_ITEM_ID_PASSWORD_ENTRY,
                  autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY,
                  autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN)));
  EXPECT_FALSE(open_args.autoselect_first_suggestion);
  EXPECT_EQ(open_args.popup_type, PopupType::kPasswords);
}

// Test that a popup without entries doesn't show "Manage all Passwords".
TEST_F(PasswordAutofillManagerTest, SuppressManageAllWithoutPasswords) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  password_autofill_manager_ = std::make_unique<PasswordAutofillManager>(
      client.mock_driver(), &autofill_client, &client);
  client.SetAccountStorageOptIn(false);

  // Show the popup and verify the suggestions.
  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));
  password_autofill_manager_->OnShowPasswordSuggestions(
      base::i18n::RIGHT_TO_LEFT, std::u16string(),
      autofill::SHOW_ALL | autofill::IS_PASSWORD_FIELD, gfx::RectF());
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(ElementsAre(
                  autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN)));
  EXPECT_FALSE(open_args.autoselect_first_suggestion);
  EXPECT_EQ(open_args.popup_type, PopupType::kPasswords);
}

// Test that the popup is updated once account-stored suggestions are unlocked.
TEST_F(PasswordAutofillManagerTest, ShowResigninButton) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);
  client.SetNeedsReSigninForAccountStorage(true);

  // Show the popup and verify the suggestions.
  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));
  password_autofill_manager_->OnShowPasswordSuggestions(
      base::i18n::RIGHT_TO_LEFT, std::u16string(),
      autofill::SHOW_ALL | autofill::IS_PASSWORD_FIELD, gfx::RectF());
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(ElementsAre(
                  autofill::POPUP_ITEM_ID_PASSWORD_ENTRY,
                  autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY,
                  autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_RE_SIGNIN)));
  EXPECT_FALSE(open_args.autoselect_first_suggestion);
  EXPECT_EQ(open_args.popup_type, PopupType::kPasswords);
}

// Test that the popup is updated once "opt in and fill" is clicked.
TEST_F(PasswordAutofillManagerTest,
       ClickOnOptInAndFillPutsPopupInWaitingState) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);
  client.SetAccountStorageOptIn(false);
  testing::Mock::VerifyAndClearExpectations(&autofill_client);

  // Accepting a suggestion should trigger a call to update the popup. The
  // update puts the unlock button into a loading state.
  std::vector<autofill::Suggestion> suggestions;
  EXPECT_CALL(
      autofill_client,
      UpdatePopup(SuggestionVectorIdsAre(ElementsAre(
                      autofill::POPUP_ITEM_ID_PASSWORD_ENTRY,
                      autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY,
                      autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN)),
                  PopupType::kPasswords))
      .WillOnce(testing::SaveArg<0>(&suggestions));
  EXPECT_CALL(client, TriggerReauthForPrimaryAccount);
  EXPECT_CALL(autofill_client, GetPopupSuggestions())
      .WillOnce(Return(CreateTestSuggestions(/*has_opt_in_and_fill=*/true,
                                             /*has_opt_in_and_generate*/ false,
                                             /*has_re_signin=*/false)));
  password_autofill_manager_->DidAcceptSuggestion(
      test_username_, autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN,
      std::string(), 1);
  ASSERT_GE(suggestions.size(), 2u);
  EXPECT_TRUE(suggestions.back().is_loading);
}

// Test that the popup is updated once "opt in and generate" is clicked.
TEST_F(PasswordAutofillManagerTest,
       ClickOnOptInAndGeneratePutsPopupInWaitingState) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);
  client.SetAccountStorageOptIn(false);
  testing::Mock::VerifyAndClearExpectations(&autofill_client);

  // Accepting a suggestion should trigger a call to update the popup. The
  // update puts the unlock-to-generate button in a loading state.
  std::vector<autofill::Suggestion> suggestions;
  EXPECT_CALL(
      autofill_client,
      UpdatePopup(
          SuggestionVectorIdsAre(ElementsAre(
              autofill::POPUP_ITEM_ID_PASSWORD_ENTRY,
              autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY,
              autofill::
                  POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN_AND_GENERATE)),
          PopupType::kPasswords))
      .WillOnce(testing::SaveArg<0>(&suggestions));
  EXPECT_CALL(client, TriggerReauthForPrimaryAccount);
  EXPECT_CALL(autofill_client, GetPopupSuggestions())
      .WillOnce(Return(CreateTestSuggestions(/*has_opt_in_and_fill=*/false,
                                             /*has_opt_in_and_generate*/ true,
                                             /*has_re_signin=*/false)));
  password_autofill_manager_->DidAcceptSuggestion(
      test_username_,
      autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN_AND_GENERATE,
      std::string(), 1);
  ASSERT_GE(suggestions.size(), 2u);
  EXPECT_TRUE(suggestions.back().is_loading);
}

// Test that the popup is updated once "opt in and fill" is clicked.
TEST_F(PasswordAutofillManagerTest, ClickOnReSiginTriggersSigninAndHides) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);
  client.SetNeedsReSigninForAccountStorage(false);
  testing::Mock::VerifyAndClearExpectations(&autofill_client);

  EXPECT_CALL(client,
              TriggerSignIn(
                  signin_metrics::AccessPoint::ACCESS_POINT_AUTOFILL_DROPDOWN));
  EXPECT_CALL(autofill_client, HideAutofillPopup);
  password_autofill_manager_->DidAcceptSuggestion(
      test_username_,
      autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_RE_SIGNIN, std::string(),
      1);
}

// Test that the popup is updated once "opt in and fill" is clicked and the
// reauth fails.
TEST_F(PasswordAutofillManagerTest, FailedOptInAndFillUpdatesPopup) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  std::vector<autofill::Suggestion> suggestions;
  InitializePasswordAutofillManager(&client, &autofill_client);
  client.SetAccountStorageOptIn(false);
  testing::Mock::VerifyAndClearExpectations(&autofill_client);

  // Accepting a suggestion should trigger a call to update the popup.
  // First the popup enters the waiting state.
  EXPECT_CALL(autofill_client, GetPopupSuggestions)
      .WillOnce(Return(CreateTestSuggestions(/*has_opt_in_and_fill=*/true,
                                             /*has_opt_in_and_generate*/ false,
                                             /*has_re_signin=*/false)));
  // As soon as the waiting state is pending, the next update resets the popup.
  EXPECT_CALL(autofill_client, UpdatePopup).WillOnce([&] {
    testing::Mock::VerifyAndClear(&autofill_client);
    EXPECT_CALL(autofill_client, GetReopenPopupArgs)
        .WillOnce(Return(CreateReopenArgsWithTestSuggestions(
            /*has_opt_in_and_fill=*/true, /*has_opt_in_and_generate*/ false,
            /*has_re_signin=*/false)));
    EXPECT_CALL(client,
                TriggerReauthForPrimaryAccount(
                    signin_metrics::ReauthAccessPoint::kAutofillDropdown, _))
        .WillOnce([](auto, auto reauth_callback) {
          std::move(reauth_callback).Run(ReauthSucceeded(false));
        });
    EXPECT_CALL(autofill_client, ShowAutofillPopup);
    EXPECT_CALL(autofill_client, PinPopupView);
    EXPECT_CALL(
        autofill_client,
        UpdatePopup(
            SuggestionVectorIdsAre(ElementsAre(
                autofill::POPUP_ITEM_ID_PASSWORD_ENTRY,
                autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY,
                autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN)),
            PopupType::kPasswords))
        .WillOnce(testing::SaveArg<0>(&suggestions));
  });

  password_autofill_manager_->DidAcceptSuggestion(
      test_username_, autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN,
      std::string(), 1);
  ASSERT_GE(suggestions.size(), 2u);
  EXPECT_FALSE(suggestions.back().is_loading);
}

// Test that the popup is updated once "opt in and generate" is clicked and the
// reauth fails.
TEST_F(PasswordAutofillManagerTest, FailedOptInAndGenerateUpdatesPopup) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  std::vector<autofill::Suggestion> suggestions;
  InitializePasswordAutofillManager(&client, &autofill_client);
  client.SetAccountStorageOptIn(false);
  testing::Mock::VerifyAndClearExpectations(&autofill_client);

  // Accepting a suggestion should trigger a call to update the popup.
  // First the popup enters the waiting state.
  EXPECT_CALL(autofill_client, GetPopupSuggestions)
      .WillOnce(Return(CreateTestSuggestions(/*has_opt_in_and_fill=*/false,
                                             /*has_opt_in_and_generate*/ true,
                                             /*has_re_signin=*/false)));
  // As soon as the waiting state is pending, the next update resets the popup.
  EXPECT_CALL(autofill_client, UpdatePopup).WillOnce([&] {
    testing::Mock::VerifyAndClear(&autofill_client);
    EXPECT_CALL(autofill_client, GetReopenPopupArgs)
        .WillOnce(Return(CreateReopenArgsWithTestSuggestions(
            /*has_opt_in_and_fill=*/false, /*has_opt_in_and_generate*/ true,
            /*has_re_signin=*/false)));
    EXPECT_CALL(
        client,
        TriggerReauthForPrimaryAccount(
            signin_metrics::ReauthAccessPoint::kGeneratePasswordDropdown, _))
        .WillOnce([](auto, auto reauth_callback) {
          std::move(reauth_callback).Run(ReauthSucceeded(false));
        });
    EXPECT_CALL(autofill_client, ShowAutofillPopup);
    EXPECT_CALL(autofill_client, PinPopupView);
    EXPECT_CALL(
        autofill_client,
        UpdatePopup(
            SuggestionVectorIdsAre(ElementsAre(
                autofill::POPUP_ITEM_ID_PASSWORD_ENTRY,
                autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY,
                autofill::
                    POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN_AND_GENERATE)),
            PopupType::kPasswords))
        .WillOnce(testing::SaveArg<0>(&suggestions));
  });

  password_autofill_manager_->DidAcceptSuggestion(
      test_username_,
      autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN_AND_GENERATE,
      std::string(), 1);
  ASSERT_GE(suggestions.size(), 2u);
  EXPECT_FALSE(suggestions.back().is_loading);
}

// Test that the popup is updated once "opt in and fill" is clicked and the
// reauth is successful.
TEST_F(PasswordAutofillManagerTest, SuccessfullOptInAndFillHidesPopup) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);
  client.SetAccountStorageOptIn(false);
  testing::Mock::VerifyAndClearExpectations(&autofill_client);

  // Accepting a suggestion should trigger a call to update the popup.
  EXPECT_CALL(autofill_client, GetPopupSuggestions)
      .WillOnce(Return(CreateTestSuggestions(/*has_opt_in_and_fill=*/true,
                                             /*has_opt_in_and_generate*/ false,
                                             /*has_re_signin=*/false)));
  EXPECT_CALL(autofill_client, UpdatePopup);
  EXPECT_CALL(autofill_client, GetReopenPopupArgs)
      .WillOnce(Return(CreateReopenArgsWithTestSuggestions(
          /*has_opt_in_and_fill=*/true, /*has_opt_in_and_generate*/ false,
          /*has_re_signin=*/false)));
  EXPECT_CALL(client,
              TriggerReauthForPrimaryAccount(
                  signin_metrics::ReauthAccessPoint::kAutofillDropdown, _))
      .WillOnce([](auto, auto reauth_callback) {
        std::move(reauth_callback).Run(ReauthSucceeded(true));
      });
  EXPECT_CALL(autofill_client, ShowAutofillPopup);
  EXPECT_CALL(autofill_client, PinPopupView);

  password_autofill_manager_->DidAcceptSuggestion(
      test_username_, autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN,
      std::string(), 1);
}

// Test that the popup is hidden and password generation is triggered once
// "opt in and generate" is clicked and the reauth is successful.
TEST_F(PasswordAutofillManagerTest,
       SuccessfullOptInAndGenerateHidesPopupAndTriggersGeneration) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);
  client.SetAccountStorageOptIn(false);
  testing::Mock::VerifyAndClearExpectations(&autofill_client);

  // Accepting a suggestion should trigger a call to update the popup.
  EXPECT_CALL(autofill_client, GetPopupSuggestions)
      .WillOnce(Return(CreateTestSuggestions(/*has_opt_in_and_fill=*/false,
                                             /*has_opt_in_and_generate*/ true,
                                             /*has_re_signin=*/false)));
  EXPECT_CALL(autofill_client, UpdatePopup);
  EXPECT_CALL(autofill_client, GetReopenPopupArgs)
      .WillOnce(Return(CreateReopenArgsWithTestSuggestions(
          /*has_opt_in_and_fill=*/false, /*has_opt_in_and_generate*/ true,
          /*has_re_signin=*/false)));
  EXPECT_CALL(
      client,
      TriggerReauthForPrimaryAccount(
          signin_metrics::ReauthAccessPoint::kGeneratePasswordDropdown, _))
      .WillOnce([](auto, auto reauth_callback) {
        std::move(reauth_callback).Run(ReauthSucceeded(true));
      });
  EXPECT_CALL(autofill_client, ShowAutofillPopup);
  EXPECT_CALL(autofill_client, PinPopupView);
  EXPECT_CALL(autofill_client,
              HideAutofillPopup(autofill::PopupHidingReason::kAcceptSuggestion))
      .Times(testing::AtLeast(1));
  EXPECT_CALL(client, GeneratePassword(PasswordGenerationType::kAutomatic));

  password_autofill_manager_->DidAcceptSuggestion(
      test_username_,
      autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN_AND_GENERATE,
      std::string(), 1);
}

// Test that the popup shows an empty state if opted-into an empty store.
TEST_F(PasswordAutofillManagerTest, SuccessfullOptInMayShowEmptyState) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  base::HistogramTester histograms;
  InitializePasswordAutofillManager(&client, &autofill_client);
  client.SetAccountStorageOptIn(true);
  testing::Mock::VerifyAndClearExpectations(&autofill_client);

  // Only the unlock button was available. After being clicked, it's in a
  // loading state which the DeleteFillData() call will end.
  Suggestion unlock_suggestion(
      /*value=*/"Unlock passwords and fill", /*label=*/"", /*icon=*/"",
      /*frontend_id=*/
      autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN);
  unlock_suggestion.is_loading = Suggestion::IsLoading(true);
  EXPECT_CALL(autofill_client, GetPopupSuggestions)
      .WillRepeatedly(Return(std::vector<Suggestion>{unlock_suggestion}));
  EXPECT_CALL(autofill_client,
              HideAutofillPopup(autofill::PopupHidingReason::kStaleData));
  EXPECT_CALL(
      autofill_client,
      UpdatePopup(SuggestionVectorIdsAre(ElementsAre(
                      autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_EMPTY)),
                  PopupType::kPasswords));

  password_autofill_manager_->DeleteFillData();
  password_autofill_manager_->OnNoCredentialsFound();
  histograms.ExpectBucketCount(
      kCredentialsCountFromAccountStoreAfterUnlockHistogram, 0, 1);
}

// Test that the popup is updated once "opt in and fill" is clicked".
TEST_F(PasswordAutofillManagerTest,
       AddOnFillDataAfterOptInAndFillPopulatesPopup) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  base::HistogramTester histograms;
  InitializePasswordAutofillManager(&client, &autofill_client);
  client.SetAccountStorageOptIn(true);
  testing::Mock::VerifyAndClearExpectations(&autofill_client);

  // Once the data is loaded, an update fills the new passwords:
  autofill::PasswordFormFillData new_data = CreateTestFormFillData();
  new_data.uses_account_store = true;
  autofill::PasswordAndMetadata additional;
  additional.realm = "https://foobarrealm.org";
  additional.username = u"bar.foo@example.com";
  new_data.additional_logins.push_back(std::move(additional));
  EXPECT_CALL(autofill_client, GetPopupSuggestions())
      .WillRepeatedly(Return(SetLoading(
          CreateTestSuggestions(
              /*has_opt_in_and_fill=*/true, /*has_opt_in_and_generate*/ false,
              /*has_re_signin=*/false),
          /*index_of_loading_element=*/2)));  // Opt-in is at third position.
  EXPECT_CALL(autofill_client,
              HideAutofillPopup(autofill::PopupHidingReason::kStaleData));
  EXPECT_CALL(
      autofill_client,
      UpdatePopup(SuggestionVectorIdsAre(ElementsAre(
                      autofill::POPUP_ITEM_ID_ACCOUNT_STORAGE_PASSWORD_ENTRY,
                      autofill::POPUP_ITEM_ID_PASSWORD_ENTRY,
                      autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY)),
                  PopupType::kPasswords));

  password_autofill_manager_->DeleteFillData();
  password_autofill_manager_->OnAddPasswordFillData(new_data);
  histograms.ExpectBucketCount(
      kCredentialsCountFromAccountStoreAfterUnlockHistogram, 1, 1);
}

// Test that OnShowPasswordSuggestions correctly matches the given FormFieldData
// to the known PasswordFormFillData, and extracts the right suggestions.
TEST_F(PasswordAutofillManagerTest, ExtractSuggestions) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data = CreateTestFormFillData();

  autofill::PasswordAndMetadata additional;
  additional.realm = "https://foobarrealm.org";
  additional.username = u"John Foo";
  data.additional_logins.push_back(additional);

  autofill::PasswordAndMetadata additional1;
  additional1.realm = "https://foobarrealm.org";
  additional1.username = u"Kohn Foo";
  data.additional_logins.push_back(std::move(additional1));

  autofill::PasswordAndMetadata additional2;
  additional2.realm = "https://foobarrealm.org";
  additional2.username = u"Gohn Foo";
  data.additional_logins.push_back(std::move(additional2));

  password_autofill_manager_->OnAddPasswordFillData(data);

  // First, simulate displaying suggestions matching an empty prefix. Also
  // verify that both the values and labels are filled correctly. The 'value'
  // should be the user name; the 'label' should be the realm.
  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));
  password_autofill_manager_->OnShowPasswordSuggestions(
      base::i18n::RIGHT_TO_LEFT, std::u16string(),
      autofill::ShowPasswordSuggestionsOptions(), element_bounds);
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorValuesAre(testing::UnorderedElementsAre(
                  test_username_, u"Gohn Foo", u"John Foo", u"Kohn Foo",
                  GetManagePasswordsTitle())));
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorLabelsAre(testing::Contains(u"foo.com")));
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorLabelsAre(testing::Contains(u"foobarrealm.org")));

  // Now simulate displaying suggestions matching "John".
  EXPECT_CALL(autofill_client, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));
  password_autofill_manager_->OnShowPasswordSuggestions(
      base::i18n::RIGHT_TO_LEFT, u"John",
      autofill::ShowPasswordSuggestionsOptions(), element_bounds);
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorValuesAre(
                  ElementsAre(additional.username, GetManagePasswordsTitle())));

  // Finally, simulate displaying all suggestions, without any prefix matching.
  EXPECT_CALL(autofill_client, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));
  password_autofill_manager_->OnShowPasswordSuggestions(
      base::i18n::RIGHT_TO_LEFT, u"xyz", autofill::SHOW_ALL, element_bounds);
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorValuesAre(
                  ElementsAre(test_username_, u"Gohn Foo", u"John Foo",
                              u"Kohn Foo", GetManagePasswordsTitle())));
}

// Verify that, for Android application credentials, the prettified realms of
// applications are displayed as the labels of suggestions on the UI (for
// matches of all levels of preferredness).
TEST_F(PasswordAutofillManagerTest, PrettifiedAndroidRealmsAreShownAsLabels) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  autofill::PasswordFormFillData data;
  data.username_field.value = test_username_;
  data.preferred_realm = "android://hash@com.example1.android/";

  autofill::PasswordAndMetadata additional;
  additional.realm = "android://hash@com.example2.android/";
  additional.username = u"John Foo";
  data.additional_logins.push_back(std::move(additional));

  password_autofill_manager_->OnAddPasswordFillData(data);

  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));
  password_autofill_manager_->OnShowPasswordSuggestions(
      base::i18n::RIGHT_TO_LEFT, std::u16string(),
      autofill::ShowPasswordSuggestionsOptions(), gfx::RectF());
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorLabelsAre(
                  testing::Contains(u"android://com.example2.android/")));
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorLabelsAre(
                  testing::Contains(u"android://com.example1.android/")));
  EXPECT_FALSE(open_args.autoselect_first_suggestion);
  EXPECT_EQ(open_args.popup_type, PopupType::kPasswords);
}

TEST_F(PasswordAutofillManagerTest, FillSuggestionPasswordField) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data = CreateTestFormFillData();

  autofill::PasswordAndMetadata additional;
  additional.realm = "https://foobarrealm.org";
  additional.username = u"John Foo";
  data.additional_logins.push_back(std::move(additional));

  password_autofill_manager_->OnAddPasswordFillData(data);

  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));
  password_autofill_manager_->OnShowPasswordSuggestions(
      base::i18n::RIGHT_TO_LEFT, test_username_, autofill::IS_PASSWORD_FIELD,
      element_bounds);
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorValuesAre(
                  ElementsAre(test_username_, GetManagePasswordsTitle())));
  EXPECT_FALSE(open_args.autoselect_first_suggestion);
  EXPECT_EQ(open_args.popup_type, PopupType::kPasswords);
}

// Verify that typing "foo" into the username field will match usernames
// "foo.bar@example.com", "bar.foo@example.com" and "example@foo.com".
TEST_F(PasswordAutofillManagerTest, DisplaySuggestionsWithMatchingTokens) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      autofill::features::kAutofillTokenPrefixMatching);

  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data;
  std::u16string username = u"foo.bar@example.com";
  data.username_field.value = username;
  data.password_field.value = u"foobar";
  data.preferred_realm = "http://foo.com/";

  autofill::PasswordAndMetadata additional;
  additional.realm = "https://foobarrealm.org";
  additional.username = u"bar.foo@example.com";
  data.additional_logins.push_back(additional);

  password_autofill_manager_->OnAddPasswordFillData(data);

  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));
  password_autofill_manager_->OnShowPasswordSuggestions(
      base::i18n::RIGHT_TO_LEFT, u"foo",
      autofill::ShowPasswordSuggestionsOptions(), element_bounds);
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorValuesAre(testing::UnorderedElementsAre(
                  username, additional.username, GetManagePasswordsTitle())));
  EXPECT_FALSE(open_args.autoselect_first_suggestion);
  EXPECT_EQ(open_args.popup_type, PopupType::kPasswords);
}

// Verify that typing "oo" into the username field will not match any usernames
// "foo.bar@example.com", "bar.foo@example.com" or "example@foo.com".
TEST_F(PasswordAutofillManagerTest, NoSuggestionForNonPrefixTokenMatch) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      autofill::features::kAutofillTokenPrefixMatching);

  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data;
  std::u16string username = u"foo.bar@example.com";
  data.username_field.value = username;
  data.password_field.value = u"foobar";
  data.preferred_realm = "http://foo.com/";

  autofill::PasswordAndMetadata additional;
  additional.realm = "https://foobarrealm.org";
  additional.username = u"bar.foo@example.com";
  data.additional_logins.push_back(std::move(additional));

  password_autofill_manager_->OnAddPasswordFillData(data);

  EXPECT_CALL(autofill_client, ShowAutofillPopup).Times(0);
  password_autofill_manager_->OnShowPasswordSuggestions(
      base::i18n::RIGHT_TO_LEFT, u"oo",
      autofill::ShowPasswordSuggestionsOptions(), element_bounds);
}

// Verify that typing "foo@exam" into the username field will match username
// "bar.foo@example.com" even if the field contents span accross multiple
// tokens.
TEST_F(PasswordAutofillManagerTest,
       MatchingContentsWithSuggestionTokenSeparator) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      autofill::features::kAutofillTokenPrefixMatching);

  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data;
  std::u16string username = u"foo.bar@example.com";
  data.username_field.value = username;
  data.password_field.value = u"foobar";
  data.preferred_realm = "http://foo.com/";

  autofill::PasswordAndMetadata additional;
  additional.realm = "https://foobarrealm.org";
  additional.username = u"bar.foo@example.com";
  data.additional_logins.push_back(additional);

  password_autofill_manager_->OnAddPasswordFillData(data);

  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));
  password_autofill_manager_->OnShowPasswordSuggestions(
      base::i18n::RIGHT_TO_LEFT, u"foo@exam",
      autofill::ShowPasswordSuggestionsOptions(), element_bounds);
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorValuesAre(
                  ElementsAre(additional.username, GetManagePasswordsTitle())));
  EXPECT_FALSE(open_args.autoselect_first_suggestion);
  EXPECT_EQ(open_args.popup_type, PopupType::kPasswords);
}

// Verify that typing "example" into the username field will match and order
// usernames "example@foo.com", "foo.bar@example.com" and "bar.foo@example.com"
// i.e. prefix matched followed by substring matched.
TEST_F(PasswordAutofillManagerTest,
       DisplaySuggestionsWithPrefixesPrecedeSubstringMatched) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      autofill::features::kAutofillTokenPrefixMatching);

  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data;
  std::u16string username = u"foo.bar@example.com";
  data.username_field.value = username;
  data.password_field.value = u"foobar";
  data.preferred_realm = "http://foo.com/";

  autofill::PasswordAndMetadata additional;
  additional.realm = "https://foobarrealm.org";
  additional.username = u"bar.foo@example.com";
  data.additional_logins.push_back(additional);

  password_autofill_manager_->OnAddPasswordFillData(data);

  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));
  password_autofill_manager_->OnShowPasswordSuggestions(
      base::i18n::RIGHT_TO_LEFT, u"foo",
      autofill::ShowPasswordSuggestionsOptions(), element_bounds);
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorValuesAre(ElementsAre(
                  username, additional.username, GetManagePasswordsTitle())));
  EXPECT_FALSE(open_args.autoselect_first_suggestion);
  EXPECT_EQ(open_args.popup_type, PopupType::kPasswords);
}

TEST_F(PasswordAutofillManagerTest, PreviewAndFillEmptyUsernameSuggestion) {
  // Initialize PasswordAutofillManager with credentials without username.
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  fill_data().username_field.value.clear();
  InitializePasswordAutofillManager(&client, &autofill_client);

  std::u16string no_username_string =
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN);

  // Simulate that the user clicks on a username field.
  EXPECT_CALL(autofill_client, ShowAutofillPopup);
  gfx::RectF element_bounds;
  password_autofill_manager_->OnShowPasswordSuggestions(
      base::i18n::RIGHT_TO_LEFT, std::u16string(),
      autofill::ShowPasswordSuggestionsOptions(), element_bounds);

  // Check that preview of the empty username works.
  EXPECT_CALL(*client.mock_driver(),
              PreviewSuggestion(std::u16string(), test_password_));
  password_autofill_manager_->DidSelectSuggestion(no_username_string,
                                                  0 /*not used*/);
  testing::Mock::VerifyAndClearExpectations(client.mock_driver());

  // Check that fill of the empty username works.
  EXPECT_CALL(*client.mock_driver(),
              FillSuggestion(std::u16string(), test_password_));
  EXPECT_CALL(
      autofill_client,
      HideAutofillPopup(autofill::PopupHidingReason::kAcceptSuggestion));
  password_autofill_manager_->DidAcceptSuggestion(
      no_username_string, autofill::POPUP_ITEM_ID_PASSWORD_ENTRY, std::string(),
      1);
  testing::Mock::VerifyAndClearExpectations(client.mock_driver());
}

// Tests that the "Manage passwords" suggestion is shown along with the password
// popup.
TEST_F(PasswordAutofillManagerTest, ShowAllPasswordsOptionOnPasswordField) {
  base::HistogramTester histograms;

  NiceMock<MockAutofillClient> autofill_client;
  auto client = std::make_unique<TestPasswordManagerClient>();
  auto manager = std::make_unique<PasswordManager>(client.get());
  InitializePasswordAutofillManager(client.get(), &autofill_client);

  ON_CALL(*(client->mock_driver()), GetPasswordManager())
      .WillByDefault(testing::Return(manager.get()));

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data = CreateTestFormFillData();

  password_autofill_manager_->OnAddPasswordFillData(data);

  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  password_autofill_manager_->OnShowPasswordSuggestions(
      base::i18n::RIGHT_TO_LEFT, test_username_, autofill::IS_PASSWORD_FIELD,
      element_bounds);
  histograms.ExpectUniqueSample(kDropdownShownHistogram,
                                metrics_util::PasswordDropdownState::kStandard,
                                1);
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorValuesAre(
                  ElementsAre(test_username_, GetManagePasswordsTitle())));

  // Clicking at the "Show all passwords row" should trigger a call to open
  // the Password Manager settings page and hide the popup.
  EXPECT_CALL(*client, NavigateToManagePasswordsPage(
                           ManagePasswordsReferrer::kPasswordDropdown));
  EXPECT_CALL(
      autofill_client,
      HideAutofillPopup(autofill::PopupHidingReason::kAcceptSuggestion));
  password_autofill_manager_->DidAcceptSuggestion(
      std::u16string(), autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY,
      std::string(), 0);
  histograms.ExpectUniqueSample(
      kDropdownSelectedHistogram,
      metrics_util::PasswordDropdownSelectedOption::kShowAll, 1);
  // Trigger UKM reporting, which happens at destruction time.
  ukm::SourceId expected_source_id = client->GetUkmSourceId();
  manager.reset();
  client.reset();

  const auto& entries = autofill_client.GetTestUkmRecorder()->GetEntriesByName(
      UkmEntry::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const auto* entry : entries) {
    EXPECT_EQ(expected_source_id, entry->source_id);
    ukm::TestUkmRecorder::ExpectEntryMetric(
        entry, UkmEntry::kPageLevelUserActionName,
        static_cast<int64_t>(
            PasswordManagerMetricsRecorder::PageLevelUserAction::
                kShowAllPasswordsWhileSomeAreSuggested));
  }
}

// Tests that the "Manage passwords" fallback shows up in non-password
// fields of login forms.
TEST_F(PasswordAutofillManagerTest, ShowAllPasswordsOptionOnNonPasswordField) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data = CreateTestFormFillData();

  password_autofill_manager_->OnAddPasswordFillData(data);

  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));
  password_autofill_manager_->OnShowPasswordSuggestions(
      base::i18n::RIGHT_TO_LEFT, test_username_,
      autofill::ShowPasswordSuggestionsOptions(), element_bounds);
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorValuesAre(
                  ElementsAre(test_username_, GetManagePasswordsTitle())));
  EXPECT_FALSE(open_args.autoselect_first_suggestion);
  EXPECT_EQ(open_args.popup_type, PopupType::kPasswords);
}

TEST_F(PasswordAutofillManagerTest,
       MaybeShowPasswordSuggestionsWithGenerationNoCredentials) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  password_autofill_manager_ = std::make_unique<PasswordAutofillManager>(
      client.mock_driver(), &autofill_client, &client);

  EXPECT_CALL(autofill_client, ShowAutofillPopup).Times(0);
  gfx::RectF element_bounds;
  EXPECT_FALSE(
      password_autofill_manager_->MaybeShowPasswordSuggestionsWithGeneration(
          element_bounds, base::i18n::RIGHT_TO_LEFT,
          /*show_password_suggestions=*/true));
}

TEST_F(PasswordAutofillManagerTest,
       MaybeShowPasswordSuggestionsWithGenerationSomeCredentials) {
  base::HistogramTester histograms;
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data = CreateTestFormFillData();

  favicon::MockFaviconService favicon_service;
  EXPECT_CALL(client, GetFaviconService()).WillOnce(Return(&favicon_service));
  EXPECT_CALL(favicon_service, GetFaviconImageForPageURL(data.url, _, _));
  password_autofill_manager_->OnAddPasswordFillData(data);

  // Bring up the drop-down with the generation option.
  std::u16string generation_string =
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_GENERATE_PASSWORD);
  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));
  EXPECT_TRUE(
      password_autofill_manager_->MaybeShowPasswordSuggestionsWithGeneration(
          element_bounds, base::i18n::RIGHT_TO_LEFT,
          /*show_password_suggestions=*/true));
  histograms.ExpectUniqueSample(
      kDropdownShownHistogram,
      metrics_util::PasswordDropdownState::kStandardGenerate, 1);
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorIconsAre(ElementsAre("globeIcon", "keyIcon",
                                                   GetManagePasswordsIcon())));
  EXPECT_THAT(open_args.suggestions, SuggestionVectorValuesAre(ElementsAre(
                                         test_username_, generation_string,
                                         GetManagePasswordsTitle())));

  // Click "Generate password".
  EXPECT_CALL(client, GeneratePassword(PasswordGenerationType::kAutomatic));
  EXPECT_CALL(
      autofill_client,
      HideAutofillPopup(autofill::PopupHidingReason::kAcceptSuggestion));
  password_autofill_manager_->DidAcceptSuggestion(
      std::u16string(), autofill::POPUP_ITEM_ID_GENERATE_PASSWORD_ENTRY,
      std::string(), 1);
  histograms.ExpectUniqueSample(
      kDropdownSelectedHistogram,
      metrics_util::PasswordDropdownSelectedOption::kGenerate, 1);
}

TEST_F(PasswordAutofillManagerTest,
       MaybeShowPasswordSuggestionsWithOmittedCredentials) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data = CreateTestFormFillData();

  favicon::MockFaviconService favicon_service;
  EXPECT_CALL(client, GetFaviconService()).WillOnce(Return(&favicon_service));
  EXPECT_CALL(favicon_service, GetFaviconImageForPageURL(data.url, _, _));
  password_autofill_manager_->OnAddPasswordFillData(data);

  std::u16string generation_string =
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_GENERATE_PASSWORD);

  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  EXPECT_TRUE(
      password_autofill_manager_->MaybeShowPasswordSuggestionsWithGeneration(
          element_bounds, base::i18n::RIGHT_TO_LEFT,
          /*show_password_suggestions=*/false));
  EXPECT_THAT(open_args.suggestions, SuggestionVectorIconsAre(ElementsAre(
                                         "keyIcon", GetManagePasswordsIcon())));
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorValuesAre(
                  ElementsAre(generation_string, GetManagePasswordsTitle())));
}

// Test that if the "opt in and generate" button gets displayed, the regular
// generation button does not.
TEST_F(PasswordAutofillManagerTest,
       MaybeShowPasswordSuggestionsWithAccountPasswordsEnabled) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kEnablePasswordsAccountStorage);

  TestPasswordManagerClient client;
  client.SetAccountStorageOptIn(false);

  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  autofill::PasswordFormFillData data = CreateTestFormFillData();

  favicon::MockFaviconService favicon_service;
  EXPECT_CALL(client, GetFaviconService()).WillOnce(Return(&favicon_service));
  EXPECT_CALL(favicon_service, GetFaviconImageForPageURL(data.url, _, _));
  password_autofill_manager_->OnAddPasswordFillData(data);

  auto opt_in_and_generate_id = static_cast<int>(
      autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN_AND_GENERATE);
  auto regular_generate_id =
      static_cast<int>(autofill::POPUP_ITEM_ID_GENERATE_PASSWORD_ENTRY);
  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  password_autofill_manager_->MaybeShowPasswordSuggestionsWithGeneration(
      gfx::RectF(), base::i18n::RIGHT_TO_LEFT,
      /*show_password_suggestions=*/true);
  EXPECT_THAT(open_args.suggestions,
              Not(Contains(Field(&autofill::Suggestion::frontend_id,
                                 Eq(regular_generate_id)))));
  EXPECT_THAT(open_args.suggestions,
              Contains(Field(&autofill::Suggestion::frontend_id,
                             Eq(opt_in_and_generate_id))));
}

TEST_F(PasswordAutofillManagerTest, DisplayAccountSuggestionsIndicatorIcon) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kEnablePasswordsAccountStorage);

  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data;
  data.username_field.value = test_username_;
  data.password_field.value = u"foobar";
  data.uses_account_store = true;

  password_autofill_manager_->OnAddPasswordFillData(data);

  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));
  password_autofill_manager_->OnShowPasswordSuggestions(
      base::i18n::RIGHT_TO_LEFT, std::u16string(),
      autofill::ShowPasswordSuggestionsOptions(), element_bounds);
  ASSERT_THAT(open_args.suggestions.size(),
              testing::Ge(1u));  // No footer on Android.
  EXPECT_THAT(open_args.suggestions[0].store_indicator_icon, "google");
  EXPECT_FALSE(open_args.autoselect_first_suggestion);
  EXPECT_EQ(open_args.popup_type, PopupType::kPasswords);
}

TEST_F(PasswordAutofillManagerTest, FillsSuggestionIfAuthNotAvailable) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  client.SetBiometricAuthenticator(authenticator_);

  InitializePasswordAutofillManager(&client, &autofill_client);

  for (bool is_suggestion_on_password_field : {false, true}) {
    SCOPED_TRACE(testing::Message() << "is_suggestion_on_password_field = "
                                    << is_suggestion_on_password_field);
    // Show the popup
    autofill::AutofillClient::PopupOpenArgs open_args;
    EXPECT_CALL(autofill_client, ShowAutofillPopup)
        .WillOnce(testing::SaveArg<0>(&open_args));

    int show_suggestion_options =
        is_suggestion_on_password_field ? autofill::IS_PASSWORD_FIELD : 0;
    password_autofill_manager_->OnShowPasswordSuggestions(
        base::i18n::RIGHT_TO_LEFT, std::u16string(), show_suggestion_options,
        gfx::RectF());
    ASSERT_GE(open_args.suggestions.size(), 1u);
    EXPECT_THAT(open_args.suggestions,
                SuggestionVectorIdsAre(ElementsAre(
                    is_suggestion_on_password_field
                        ? autofill::POPUP_ITEM_ID_PASSWORD_ENTRY
                        : autofill::POPUP_ITEM_ID_USERNAME_ENTRY,
                    autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY)));

    // Suggestions should always be filled if the authenticator is not available
    // or it cannot be used.
    EXPECT_CALL(*client.mock_driver(),
                FillSuggestion(test_username_, test_password_));

    // The authenticator exists, but cannot be used for authentication.
    EXPECT_CALL(*authenticator_.get(), CanAuthenticate())
        .WillOnce(Return(BiometricsAvailability::kNoHardware));

    // Accept the suggestion to start the filing process which tries to
    // reauthenticate the user if possible.
    password_autofill_manager_->DidAcceptSuggestion(
        test_username_, autofill::POPUP_ITEM_ID_PASSWORD_ENTRY, std::string(),
        1);
  }
}

TEST_F(PasswordAutofillManagerTest, FillsSuggestionIfAuthSuccessful) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kBiometricTouchToFill);
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  client.SetBiometricAuthenticator(authenticator_);

  InitializePasswordAutofillManager(&client, &autofill_client);

  for (bool is_suggestion_on_password_field : {false, true}) {
    SCOPED_TRACE(testing::Message() << "is_suggestion_on_password_field = "
                                    << is_suggestion_on_password_field);
    // Show the popup
    autofill::AutofillClient::PopupOpenArgs open_args;
    EXPECT_CALL(autofill_client, ShowAutofillPopup)
        .WillOnce(testing::SaveArg<0>(&open_args));

    int show_suggestion_options =
        is_suggestion_on_password_field ? autofill::IS_PASSWORD_FIELD : 0;
    password_autofill_manager_->OnShowPasswordSuggestions(
        base::i18n::RIGHT_TO_LEFT, std::u16string(), show_suggestion_options,
        gfx::RectF());
    ASSERT_GE(open_args.suggestions.size(), 1u);
    EXPECT_THAT(open_args.suggestions,
                SuggestionVectorIdsAre(ElementsAre(
                    is_suggestion_on_password_field
                        ? autofill::POPUP_ITEM_ID_PASSWORD_ENTRY
                        : autofill::POPUP_ITEM_ID_USERNAME_ENTRY,
                    autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY)));

    // The suggestion should be filled if the authentication is successful.
    EXPECT_CALL(*client.mock_driver(),
                FillSuggestion(test_username_, test_password_));

    // Accepting a suggestion should trigger a call to hide the popup.
    EXPECT_CALL(
        autofill_client,
        HideAutofillPopup(autofill::PopupHidingReason::kAcceptSuggestion));

    // The authenticator exists and is available.
    EXPECT_CALL(*authenticator_.get(), CanAuthenticate())
        .WillOnce(Return(BiometricsAvailability::kAvailable));
    EXPECT_CALL(*authenticator_.get(),
                Authenticate(BiometricAuthRequester::kAutofillSuggestion, _))
        .WillOnce(RunOnceCallback<1>(/*auth_succeeded=*/true));

    // Accept the suggestion to start the filing process which tries to
    // reauthenticate the user if possible.
    password_autofill_manager_->DidAcceptSuggestion(
        test_username_, autofill::POPUP_ITEM_ID_PASSWORD_ENTRY, std::string(),
        1);
  }
}

TEST_F(PasswordAutofillManagerTest, DoesntFillSuggestionIfAuthFailed) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kBiometricTouchToFill);
  for (bool is_suggestion_on_password_field : {false, true}) {
    TestPasswordManagerClient client;
    NiceMock<MockAutofillClient> autofill_client;
    client.SetBiometricAuthenticator(authenticator_);

    InitializePasswordAutofillManager(&client, &autofill_client);

    SCOPED_TRACE(testing::Message() << "is_suggestion_on_password_field = "
                                    << is_suggestion_on_password_field);
    // Show the popup
    autofill::AutofillClient::PopupOpenArgs open_args;
    EXPECT_CALL(autofill_client, ShowAutofillPopup)
        .WillOnce(testing::SaveArg<0>(&open_args));

    int show_suggestion_options =
        is_suggestion_on_password_field ? autofill::IS_PASSWORD_FIELD : 0;
    password_autofill_manager_->OnShowPasswordSuggestions(
        base::i18n::RIGHT_TO_LEFT, std::u16string(), show_suggestion_options,
        gfx::RectF());
    ASSERT_GE(open_args.suggestions.size(), 1u);
    EXPECT_THAT(open_args.suggestions,
                SuggestionVectorIdsAre(ElementsAre(
                    is_suggestion_on_password_field
                        ? autofill::POPUP_ITEM_ID_PASSWORD_ENTRY
                        : autofill::POPUP_ITEM_ID_USERNAME_ENTRY,
                    autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY)));

    // The suggestion should not be filled if the authentication fails.
    EXPECT_CALL(*client.mock_driver(),
                FillSuggestion(test_username_, test_password_))
        .Times(0);
    // Accepting a suggestion should trigger a call to hide the popup.
    EXPECT_CALL(
        autofill_client,
        HideAutofillPopup(autofill::PopupHidingReason::kAcceptSuggestion));

    // The authenticator exists and is available.
    EXPECT_CALL(*authenticator_.get(), CanAuthenticate())
        .WillOnce(Return(BiometricsAvailability::kAvailable));
    EXPECT_CALL(*authenticator_.get(),
                Authenticate(BiometricAuthRequester::kAutofillSuggestion, _))
        .WillOnce(RunOnceCallback<1>(/*auth_succeeded=*/false));

    // Accept the suggestion to start the filing process which tries to
    // reauthenticate the user if possible.
    password_autofill_manager_->DidAcceptSuggestion(
        test_username_, autofill::POPUP_ITEM_ID_PASSWORD_ENTRY, std::string(),
        1);
  }
}

TEST_F(PasswordAutofillManagerTest, CancelsOngoingBiometricAuthOnDestroy) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kBiometricTouchToFill);
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  client.SetBiometricAuthenticator(authenticator_);

  InitializePasswordAutofillManager(&client, &autofill_client);

  // Show the popup
  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  password_autofill_manager_->OnShowPasswordSuggestions(
      base::i18n::RIGHT_TO_LEFT, std::u16string(), autofill::IS_PASSWORD_FIELD,
      gfx::RectF());
  ASSERT_GE(open_args.suggestions.size(), 1u);
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(ElementsAre(
                  autofill::POPUP_ITEM_ID_PASSWORD_ENTRY,
                  autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY)));
  EXPECT_CALL(*client.mock_driver(),
              FillSuggestion(test_username_, test_password_))
      .Times(0);

  // The authenticator exists and is available.
  EXPECT_CALL(*authenticator_.get(), CanAuthenticate())
      .WillOnce(Return(BiometricsAvailability::kAvailable));
  EXPECT_CALL(*authenticator_.get(),
              Authenticate(BiometricAuthRequester::kAutofillSuggestion, _));

  // Accept the suggestion to start the filing process which tries to
  // reauthenticate the user if possible.
  password_autofill_manager_->DidAcceptSuggestion(
      test_username_, autofill::POPUP_ITEM_ID_PASSWORD_ENTRY, std::string(), 1);

  EXPECT_CALL(*authenticator_.get(),
              Cancel(BiometricAuthRequester::kAutofillSuggestion));
}

TEST_F(PasswordAutofillManagerTest,
       CancelsOngoingBiometricAuthOnDeleteFillData) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kBiometricTouchToFill);
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  client.SetBiometricAuthenticator(authenticator_);

  InitializePasswordAutofillManager(&client, &autofill_client);

  // Show the popup
  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  password_autofill_manager_->OnShowPasswordSuggestions(
      base::i18n::RIGHT_TO_LEFT, std::u16string(), autofill::IS_PASSWORD_FIELD,
      gfx::RectF());
  ASSERT_GE(open_args.suggestions.size(), 1u);
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(ElementsAre(
                  autofill::POPUP_ITEM_ID_PASSWORD_ENTRY,
                  autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY)));
  EXPECT_CALL(*client.mock_driver(),
              FillSuggestion(test_username_, test_password_))
      .Times(0);

  // The authenticator exists and is available.
  EXPECT_CALL(*authenticator_.get(), CanAuthenticate())
      .WillOnce(Return(BiometricsAvailability::kAvailable));
  EXPECT_CALL(*authenticator_.get(),
              Authenticate(BiometricAuthRequester::kAutofillSuggestion, _));

  // Accept the suggestion to start the filing process which tries to
  // reauthenticate the user if possible.
  password_autofill_manager_->DidAcceptSuggestion(
      test_username_, autofill::POPUP_ITEM_ID_PASSWORD_ENTRY, std::string(), 1);

  EXPECT_CALL(*authenticator_.get(),
              Cancel(BiometricAuthRequester::kAutofillSuggestion));
  password_autofill_manager_->DeleteFillData();
}

TEST_F(PasswordAutofillManagerTest,
       CancelsOngoingBiometricAuthOnFillDataChange) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kBiometricTouchToFill);
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  client.SetBiometricAuthenticator(authenticator_);

  InitializePasswordAutofillManager(&client, &autofill_client);

  // Show the popup
  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  password_autofill_manager_->OnShowPasswordSuggestions(
      base::i18n::RIGHT_TO_LEFT, std::u16string(), autofill::IS_PASSWORD_FIELD,
      gfx::RectF());
  ASSERT_GE(open_args.suggestions.size(), 1u);
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(ElementsAre(
                  autofill::POPUP_ITEM_ID_PASSWORD_ENTRY,
                  autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY)));
  EXPECT_CALL(*client.mock_driver(),
              FillSuggestion(test_username_, test_password_))
      .Times(0);

  // The authenticator exists and is available.
  EXPECT_CALL(*authenticator_.get(), CanAuthenticate())
      .WillOnce(Return(BiometricsAvailability::kAvailable));
  EXPECT_CALL(*authenticator_.get(),
              Authenticate(BiometricAuthRequester::kAutofillSuggestion, _));

  // Accept the suggestion to start the filing process which tries to
  // reauthenticate the user if possible.
  password_autofill_manager_->DidAcceptSuggestion(
      test_username_, autofill::POPUP_ITEM_ID_PASSWORD_ENTRY, std::string(), 1);

  EXPECT_CALL(*authenticator_.get(),
              Cancel(BiometricAuthRequester::kAutofillSuggestion));
  password_autofill_manager_->OnAddPasswordFillData(CreateTestFormFillData());
}

TEST_F(PasswordAutofillManagerTest, ShowsWebAuthnSuggestions) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  MockWebAuthnCredentialsDelegate webauthn_credentials_delegate;
  InitializePasswordAutofillManager(&client, &autofill_client);

  // Return a WebAuthn credential.
  const std::string kId = "abcd";
  const std::u16string kName = u"nadeshiko@example.com";
  const std::u16string kDisplayName = u"Nadeshiko Kagamihara";
  autofill::Suggestion webauthn_credential(kDisplayName);
  webauthn_credential.frontend_id = autofill::POPUP_ITEM_ID_WEBAUTHN_CREDENTIAL;
  webauthn_credential.backend_id = kId;
  webauthn_credential.label = kName;
  ON_CALL(webauthn_credentials_delegate, IsWebAuthnAutofillEnabled)
      .WillByDefault(Return(true));
  EXPECT_CALL(client, GetWebAuthnCredentialsDelegate)
      .WillRepeatedly(Return(&webauthn_credentials_delegate));
  EXPECT_CALL(webauthn_credentials_delegate, GetWebAuthnSuggestions)
      .WillOnce(Return(std::vector<autofill::Suggestion>{webauthn_credential}));

  // Show password suggestions including WebAuthn credentials.
  autofill::AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));
  gfx::RectF element_bounds;
  password_autofill_manager_->OnShowPasswordSuggestions(
      base::i18n::RIGHT_TO_LEFT, /*typed_username=*/std::u16string(),
      autofill::ShowPasswordSuggestionsOptions::ACCEPTS_WEBAUTHN_CREDENTIALS,
      element_bounds);
  ASSERT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(ElementsAre(
                  autofill::POPUP_ITEM_ID_WEBAUTHN_CREDENTIAL,
                  autofill::POPUP_ITEM_ID_USERNAME_ENTRY,
                  autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY)));
  EXPECT_EQ(open_args.suggestions[0].backend_id, kId);
  EXPECT_EQ(open_args.suggestions[0].frontend_id,
            autofill::POPUP_ITEM_ID_WEBAUTHN_CREDENTIAL);
  EXPECT_EQ(open_args.suggestions[0].value, kDisplayName);
  EXPECT_EQ(open_args.suggestions[0].label, kName);
  testing::Mock::VerifyAndClearExpectations(client.mock_driver());

  // Check that preview of the "username" (i.e. the credential name) works.
  EXPECT_CALL(*client.mock_driver(),
              PreviewSuggestion(kName, /*password=*/std::u16string(u"")));
  password_autofill_manager_->DidSelectSuggestion(
      kName, autofill::POPUP_ITEM_ID_WEBAUTHN_CREDENTIAL);
  testing::Mock::VerifyAndClearExpectations(client.mock_driver());

  // Check that selecting the credential reports back to the client.
  EXPECT_CALL(webauthn_credentials_delegate, SelectWebAuthnCredential(kId));
  EXPECT_CALL(
      autofill_client,
      HideAutofillPopup(autofill::PopupHidingReason::kAcceptSuggestion));
  password_autofill_manager_->DidAcceptSuggestion(
      kName, autofill::POPUP_ITEM_ID_WEBAUTHN_CREDENTIAL, kId, /*position=*/1);
}

}  // namespace password_manager
