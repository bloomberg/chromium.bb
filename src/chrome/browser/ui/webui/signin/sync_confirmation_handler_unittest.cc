// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/sync_confirmation_handler.h"

#include <memory>
#include <unordered_map>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/scoped_observer.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/values.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/consent_auditor/consent_auditor_test_utils.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/sync_confirmation_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/dialog_test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "components/consent_auditor/fake_consent_auditor.h"
#include "components/signin/core/browser/avatar_icon_util.h"
#include "components/unified_consent/scoped_unified_consent.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_web_ui.h"

const int kExpectedProfileImageSize = 128;

// The dialog needs to be initialized with a height but the actual value doesn't
// really matter in unit tests.
const double kDefaultDialogHeight = 350.0;

class TestingSyncConfirmationHandler : public SyncConfirmationHandler {
 public:
  TestingSyncConfirmationHandler(
      Browser* browser,
      content::WebUI* web_ui,
      std::unordered_map<std::string, int> string_to_grd_id_map)
      : SyncConfirmationHandler(browser,
                                string_to_grd_id_map,
                                consent_auditor::Feature::CHROME_SYNC) {
    set_web_ui(web_ui);
  }

  using SyncConfirmationHandler::HandleConfirm;
  using SyncConfirmationHandler::HandleUndo;
  using SyncConfirmationHandler::HandleInitializedWithSize;
  using SyncConfirmationHandler::HandleGoToSettings;
  using SyncConfirmationHandler::RecordConsent;
  using SyncConfirmationHandler::SetUserImageURL;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestingSyncConfirmationHandler);
};

class SyncConfirmationHandlerTest : public BrowserWithTestWindowTest,
                                    public LoginUIService::Observer {
 public:
  static const char kConsentText1[];
  static const char kConsentText2[];
  static const char kConsentText3[];
  static const char kConsentText4[];
  static const char kConsentText5[];

  SyncConfirmationHandlerTest()
      : did_user_explicitly_interact(false),
        on_sync_confirmation_ui_closed_called_(false),
        sync_confirmation_ui_closed_result_(LoginUIService::ABORT_SIGNIN),
        web_ui_(new content::TestWebUI),
        login_ui_service_observer_(this) {}

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    chrome::NewTab(browser());
    web_ui()->set_web_contents(
        browser()->tab_strip_model()->GetActiveWebContents());

    auto handler = std::make_unique<TestingSyncConfirmationHandler>(
        browser(), web_ui(), GetStringToGrdIdMap());
    handler_ = handler.get();
    sync_confirmation_ui_.reset(new SyncConfirmationUI(web_ui()));
    web_ui()->AddMessageHandler(std::move(handler));

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());
    account_info_ =
        identity_test_env()->MakePrimaryAccountAvailable("foo@example.com");
    login_ui_service_observer_.Add(
        LoginUIServiceFactory::GetForProfile(profile()));
  }

  void TearDown() override {
    login_ui_service_observer_.RemoveAll();
    sync_confirmation_ui_.reset();
    web_ui_.reset();
    identity_test_env_adaptor_.reset();
    BrowserWithTestWindowTest::TearDown();

    EXPECT_EQ(did_user_explicitly_interact ? 0 : 1,
              user_action_tester()->GetActionCount("Signin_Abort_Signin"));
  }

  TestingSyncConfirmationHandler* handler() { return handler_; }

  content::TestWebUI* web_ui() {
    return web_ui_.get();
  }

  base::UserActionTester* user_action_tester() {
    return &user_action_tester_;
  }

  consent_auditor::FakeConsentAuditor* consent_auditor() {
    return static_cast<consent_auditor::FakeConsentAuditor*>(
        ConsentAuditorFactory::GetForProfile(profile()));
  }

  identity::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

  std::unique_ptr<BrowserWindow> CreateBrowserWindow() override {
    return std::make_unique<DialogTestBrowserWindow>();
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    TestingProfile::TestingFactories factories = {
        {ConsentAuditorFactory::GetInstance(),
         base::BindRepeating(&BuildFakeConsentAuditor)}};
    IdentityTestEnvironmentProfileAdaptor::
        AppendIdentityTestEnvironmentFactories(&factories);
    return factories;
  }

  const std::unordered_map<std::string, int>& GetStringToGrdIdMap() {
    if (string_to_grd_id_map_.empty()) {
      string_to_grd_id_map_[kConsentText1] = 1;
      string_to_grd_id_map_[kConsentText2] = 2;
      string_to_grd_id_map_[kConsentText3] = 3;
      string_to_grd_id_map_[kConsentText4] = 4;
      string_to_grd_id_map_[kConsentText5] = 5;
    }
    return string_to_grd_id_map_;
  }

  // LoginUIService::Observer:
  void OnSyncConfirmationUIClosed(
      LoginUIService::SyncConfirmationUIClosedResult result) override {
    on_sync_confirmation_ui_closed_called_ = true;
    sync_confirmation_ui_closed_result_ = result;
  }

  void ExpectAccountImageChanged(
      const content::TestWebUI::CallData& call_data) {
    EXPECT_EQ("cr.webUIListenerCallback", call_data.function_name());
    std::string event;
    ASSERT_TRUE(call_data.arg1()->GetAsString(&event));
    EXPECT_EQ("account-image-changed", event);

    identity::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile());
    base::Optional<AccountInfo> primary_account =
        identity_manager->FindExtendedAccountInfoForAccount(
            identity_manager->GetPrimaryAccountInfo());

    std::string original_picture_url =
        primary_account ? primary_account->picture_url : std::string();
    std::string expected_picture_url =
        original_picture_url.empty()
            ? profiles::GetPlaceholderAvatarIconUrl()
            : signin::GetAvatarImageURLWithOptions(GURL(original_picture_url),
                                                   kExpectedProfileImageSize,
                                                   false /* no_silhouette */)
                  .spec();
    std::string passed_picture_url;
    ASSERT_TRUE(call_data.arg2()->GetAsString(&passed_picture_url));
    EXPECT_EQ(expected_picture_url, passed_picture_url);
  }

 protected:
  bool did_user_explicitly_interact;
  bool on_sync_confirmation_ui_closed_called_;
  LoginUIService::SyncConfirmationUIClosedResult
      sync_confirmation_ui_closed_result_;
  // Holds information for the account currently logged in.
  AccountInfo account_info_;

 private:
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<SyncConfirmationUI> sync_confirmation_ui_;
  TestingSyncConfirmationHandler* handler_;  // Not owned.
  base::UserActionTester user_action_tester_;
  std::unordered_map<std::string, int> string_to_grd_id_map_;
  ScopedObserver<LoginUIService, LoginUIService::Observer>
      login_ui_service_observer_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;

  DISALLOW_COPY_AND_ASSIGN(SyncConfirmationHandlerTest);
};

class SyncConfirmationHandlerTest_UnifiedConsentDisabled
    : public SyncConfirmationHandlerTest {
 public:
  SyncConfirmationHandlerTest_UnifiedConsentDisabled()
      : scoped_unified_consent_(
            unified_consent::UnifiedConsentFeatureState::kDisabled) {}

 private:
  unified_consent::ScopedUnifiedConsent scoped_unified_consent_;

  DISALLOW_COPY_AND_ASSIGN(SyncConfirmationHandlerTest_UnifiedConsentDisabled);
};

const char SyncConfirmationHandlerTest::kConsentText1[] = "consentText1";
const char SyncConfirmationHandlerTest::kConsentText2[] = "consentText2";
const char SyncConfirmationHandlerTest::kConsentText3[] = "consentText3";
const char SyncConfirmationHandlerTest::kConsentText4[] = "consentText4";
const char SyncConfirmationHandlerTest::kConsentText5[] = "consentText5";

TEST_F(SyncConfirmationHandlerTest_UnifiedConsentDisabled,
       TestSetImageIfPrimaryAccountReady) {
  identity_test_env()->SimulateSuccessfulFetchOfAccountInfo(
      account_info_.account_id, account_info_.email, account_info_.gaia, "",
      "full_name", "given_name", "locale",
      "http://picture.example.com/picture.jpg");

  base::ListValue args;
  args.Set(0, std::make_unique<base::Value>(kDefaultDialogHeight));
  handler()->HandleInitializedWithSize(&args);
  EXPECT_EQ(2U, web_ui()->call_data().size());

  // When the primary account is ready, setUserImageURL happens before
  // clearFocus since the image URL is known before showing the dialog.
  EXPECT_EQ("sync.confirmation.setUserImageURL",
            web_ui()->call_data()[0]->function_name());
  EXPECT_TRUE(web_ui()->call_data()[0]->arg1()->is_string());
  std::string passed_picture_url;
  EXPECT_TRUE(
      web_ui()->call_data()[0]->arg1()->GetAsString(&passed_picture_url));

  EXPECT_EQ("sync.confirmation.clearFocus",
            web_ui()->call_data()[1]->function_name());

  identity::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());
  base::Optional<AccountInfo> primary_account_info =
      identity_manager->FindExtendedAccountInfoForAccount(
          identity_manager->GetPrimaryAccountInfo());
  std::string original_picture_url =
      primary_account_info ? primary_account_info->picture_url : std::string();
  GURL picture_url_with_size = signin::GetAvatarImageURLWithOptions(
      GURL(original_picture_url), kExpectedProfileImageSize,
      false /* no_silhouette */);
  EXPECT_EQ(picture_url_with_size.spec(), passed_picture_url);
}

TEST_F(SyncConfirmationHandlerTest, TestSetImageIfPrimaryAccountReady) {
  identity_test_env()->SimulateSuccessfulFetchOfAccountInfo(
      account_info_.account_id, account_info_.email, account_info_.gaia, "",
      "full_name", "given_name", "locale",
      "http://picture.example.com/picture.jpg");

  base::ListValue args;
  args.Set(0, std::make_unique<base::Value>(kDefaultDialogHeight));
  handler()->HandleInitializedWithSize(&args);

  ExpectAccountImageChanged(*web_ui()->call_data()[0]);
  EXPECT_EQ("sync.confirmation.clearFocus",
            web_ui()->call_data()[1]->function_name());
}

TEST_F(SyncConfirmationHandlerTest_UnifiedConsentDisabled,
       TestSetImageIfPrimaryAccountReadyLater) {
  base::ListValue args;
  args.Set(0, std::make_unique<base::Value>(kDefaultDialogHeight));
  handler()->HandleInitializedWithSize(&args);
  EXPECT_EQ(2U, web_ui()->call_data().size());

  identity_test_env()->SimulateSuccessfulFetchOfAccountInfo(
      account_info_.account_id, account_info_.email, account_info_.gaia, "",
      "full_name", "given_name", "locale",
      "http://picture.example.com/picture.jpg");

  EXPECT_EQ(3U, web_ui()->call_data().size());

  // When the primary account isn't yet ready when the dialog is shown,
  // setUserImageURL is called with the default placeholder image.
  EXPECT_EQ("sync.confirmation.setUserImageURL",
            web_ui()->call_data()[0]->function_name());
  EXPECT_TRUE(web_ui()->call_data()[0]->arg1()->is_string());
  std::string passed_picture_url;
  EXPECT_TRUE(
      web_ui()->call_data()[0]->arg1()->GetAsString(&passed_picture_url));
  EXPECT_EQ(profiles::GetPlaceholderAvatarIconUrl(), passed_picture_url);

  // When the primary account isn't yet ready when the dialog is shown,
  // clearFocus is called before the second call to setUserImageURL.
  EXPECT_EQ("sync.confirmation.clearFocus",
            web_ui()->call_data()[1]->function_name());

  EXPECT_EQ("sync.confirmation.setUserImageURL",
            web_ui()->call_data()[2]->function_name());
  EXPECT_TRUE(web_ui()->call_data()[2]->arg1()->is_string());
  EXPECT_TRUE(
      web_ui()->call_data()[2]->arg1()->GetAsString(&passed_picture_url));

  identity::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());
  base::Optional<AccountInfo> primary_account_info =
      identity_manager->FindExtendedAccountInfoForAccount(
          identity_manager->GetPrimaryAccountInfo());

  std::string original_picture_url =
      primary_account_info ? primary_account_info->picture_url : std::string();
  GURL picture_url_with_size = signin::GetAvatarImageURLWithOptions(
      GURL(original_picture_url), kExpectedProfileImageSize,
      false /* no_silhouette */);
  EXPECT_EQ(picture_url_with_size.spec(), passed_picture_url);
}

TEST_F(SyncConfirmationHandlerTest, TestSetImageIfPrimaryAccountReadyLater) {
  base::ListValue args;
  args.Set(0, std::make_unique<base::Value>(kDefaultDialogHeight));
  handler()->HandleInitializedWithSize(&args);

  EXPECT_EQ(2U, web_ui()->call_data().size());
  ExpectAccountImageChanged(*web_ui()->call_data()[0]);
  EXPECT_EQ("sync.confirmation.clearFocus",
            web_ui()->call_data()[1]->function_name());

  identity_test_env()->SimulateSuccessfulFetchOfAccountInfo(
      account_info_.account_id, account_info_.email, account_info_.gaia, "",
      "full_name", "given_name", "locale",
      "http://picture.example.com/picture.jpg");

  EXPECT_EQ(3U, web_ui()->call_data().size());
  ExpectAccountImageChanged(*web_ui()->call_data()[2]);
}

TEST_F(SyncConfirmationHandlerTest_UnifiedConsentDisabled,
       TestSetImageIgnoredIfSecondaryAccountUpdated) {
  base::ListValue args;
  args.Set(0, std::make_unique<base::Value>(kDefaultDialogHeight));
  handler()->HandleInitializedWithSize(&args);
  EXPECT_EQ(2U, web_ui()->call_data().size());

  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("bar@example.com");
  identity_test_env()->SimulateSuccessfulFetchOfAccountInfo(
      account_info.account_id, account_info.email, account_info.gaia, "",
      "bar_full_name", "bar_given_name", "bar_locale",
      "http://picture.example.com/bar_picture.jpg");

  // Updating the account info of a secondary account should not update the
  // image of the sync confirmation dialog.
  EXPECT_EQ(2U, web_ui()->call_data().size());

  identity_test_env()->SimulateSuccessfulFetchOfAccountInfo(
      account_info_.account_id, account_info_.email, account_info_.gaia, "",
      "full_name", "given_name", "locale",
      "http://picture.example.com/picture.jpg");

  // Updating the account info of the primary account should update the
  // image of the sync confirmation dialog.
  EXPECT_EQ(3U, web_ui()->call_data().size());
  EXPECT_EQ("sync.confirmation.setUserImageURL",
            web_ui()->call_data()[2]->function_name());
}

TEST_F(SyncConfirmationHandlerTest,
       TestSetImageIgnoredIfSecondaryAccountUpdated) {
  base::ListValue args;
  args.Set(0, std::make_unique<base::Value>(kDefaultDialogHeight));
  handler()->HandleInitializedWithSize(&args);
  EXPECT_EQ(2U, web_ui()->call_data().size());

  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("bar@example.com");
  identity_test_env()->SimulateSuccessfulFetchOfAccountInfo(
      account_info.account_id, account_info.email, account_info.gaia, "",
      "bar_full_name", "bar_given_name", "bar_locale",
      "http://picture.example.com/bar_picture.jpg");

  // Updating the account info of a secondary account should not update the
  // image of the sync confirmation dialog.
  EXPECT_EQ(2U, web_ui()->call_data().size());

  identity_test_env()->SimulateSuccessfulFetchOfAccountInfo(
      account_info_.account_id, account_info_.email, account_info_.gaia, "",
      "full_name", "given_name", "locale",
      "http://picture.example.com/picture.jpg");

  // Updating the account info of the primary account should update the
  // image of the sync confirmation dialog.
  EXPECT_EQ(3U, web_ui()->call_data().size());
  ExpectAccountImageChanged(*web_ui()->call_data()[2]);
}

TEST_F(SyncConfirmationHandlerTest, TestHandleUndo) {
  handler()->HandleUndo(nullptr);
  did_user_explicitly_interact = true;

  EXPECT_TRUE(on_sync_confirmation_ui_closed_called_);
  EXPECT_EQ(LoginUIService::ABORT_SIGNIN, sync_confirmation_ui_closed_result_);
  EXPECT_EQ(1, user_action_tester()->GetActionCount("Signin_Undo_Signin"));
  EXPECT_EQ(0, user_action_tester()->GetActionCount(
      "Signin_Signin_WithDefaultSyncSettings"));
  EXPECT_EQ(0, user_action_tester()->GetActionCount(
      "Signin_Signin_WithAdvancedSyncSettings"));
}

TEST_F(SyncConfirmationHandlerTest, TestHandleConfirm) {
  // The consent description consists of strings 1, 2, and 4.
  base::ListValue consent_description;
  consent_description.GetList().push_back(
      base::Value(SyncConfirmationHandlerTest::kConsentText1));
  consent_description.GetList().push_back(
      base::Value(SyncConfirmationHandlerTest::kConsentText2));
  consent_description.GetList().push_back(
      base::Value(SyncConfirmationHandlerTest::kConsentText4));

  // The consent confirmation contains string 5.
  base::Value consent_confirmation(SyncConfirmationHandlerTest::kConsentText5);

  // These are passed as parameters to HandleConfirm().
  base::ListValue args;
  args.GetList().push_back(std::move(consent_description));
  args.GetList().push_back(std::move(consent_confirmation));

  handler()->HandleConfirm(&args);
  did_user_explicitly_interact = true;

  EXPECT_TRUE(on_sync_confirmation_ui_closed_called_);
  EXPECT_EQ(LoginUIService::SYNC_WITH_DEFAULT_SETTINGS,
            sync_confirmation_ui_closed_result_);
  EXPECT_EQ(0, user_action_tester()->GetActionCount("Signin_Undo_Signin"));
  EXPECT_EQ(1, user_action_tester()->GetActionCount(
      "Signin_Signin_WithDefaultSyncSettings"));
  EXPECT_EQ(0, user_action_tester()->GetActionCount(
      "Signin_Signin_WithAdvancedSyncSettings"));

  // The corresponding string IDs get recorded.
  std::vector<std::vector<int>> expected_id_vectors = {{1, 2, 4}};
  std::vector<int> expected_confirmation_ids = {5};

  EXPECT_EQ(expected_id_vectors, consent_auditor()->recorded_id_vectors());
  EXPECT_EQ(expected_confirmation_ids,
            consent_auditor()->recorded_confirmation_ids());

  EXPECT_EQ(account_info_.account_id, consent_auditor()->account_id());
}

TEST_F(SyncConfirmationHandlerTest, TestHandleConfirmWithAdvancedSyncSettings) {
  // The consent description consists of strings 2, 3, and 5.
  base::ListValue consent_description;
  consent_description.GetList().push_back(
      base::Value(SyncConfirmationHandlerTest::kConsentText2));
  consent_description.GetList().push_back(
      base::Value(SyncConfirmationHandlerTest::kConsentText3));
  consent_description.GetList().push_back(
      base::Value(SyncConfirmationHandlerTest::kConsentText5));

  // The consent confirmation contains string 2.
  base::Value consent_confirmation(SyncConfirmationHandlerTest::kConsentText2);

  // These are passed as parameters to HandleGoToSettings().
  base::ListValue args;
  args.GetList().push_back(std::move(consent_description));
  args.GetList().push_back(std::move(consent_confirmation));

  handler()->HandleGoToSettings(&args);
  did_user_explicitly_interact = true;

  EXPECT_TRUE(on_sync_confirmation_ui_closed_called_);
  EXPECT_EQ(LoginUIService::CONFIGURE_SYNC_FIRST,
            sync_confirmation_ui_closed_result_);
  EXPECT_EQ(0, user_action_tester()->GetActionCount("Signin_Undo_Signin"));
  EXPECT_EQ(0, user_action_tester()->GetActionCount(
                   "Signin_Signin_WithDefaultSyncSettings"));
  EXPECT_EQ(1, user_action_tester()->GetActionCount(
                   "Signin_Signin_WithAdvancedSyncSettings"));

  // The corresponding string IDs get recorded.
  std::vector<std::vector<int>> expected_id_vectors = {{2, 3, 5}};
  std::vector<int> expected_confirmation_ids = {2};
  EXPECT_EQ(expected_id_vectors, consent_auditor()->recorded_id_vectors());
  EXPECT_EQ(expected_confirmation_ids,
            consent_auditor()->recorded_confirmation_ids());

  EXPECT_EQ(account_info_.account_id, consent_auditor()->account_id());
}
