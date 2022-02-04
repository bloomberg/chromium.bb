// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/files/file_enumerator.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/automated_tests/cache_replayer.h"
#include "chrome/browser/autofill/captured_sites_test_utils.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_manager_test_base.h"
#include "chrome/browser/password_manager/password_manager_uitest_util.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/password_manager/core/browser/fake_password_store_backend.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/sync/driver/test_sync_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"

using captured_sites_test_utils::CapturedSiteParams;
using captured_sites_test_utils::GetCapturedSites;
using captured_sites_test_utils::GetParamAsString;

namespace {

constexpr base::TimeDelta kWaitForSaveFallbackInterval = base::Seconds(5);

// Return path to the Password Manager captured sites test root directory. The
// directory contains subdirectories for different password manager test
// scenarios. The test scenario subdirectories contain site capture files
// and test recipe replay files.
base::FilePath GetReplayFilesRootDirectory() {
  base::FilePath src_dir;
  if (base::PathService::Get(base::DIR_SOURCE_ROOT, &src_dir)) {
    return src_dir.AppendASCII("chrome")
        .AppendASCII("test")
        .AppendASCII("data")
        .AppendASCII("password")
        .AppendASCII("captured_sites");
  }

  ADD_FAILURE() << "Unable to obtain the Chromium src directory!";
  src_dir.clear();
  return src_dir;
}

std::unique_ptr<KeyedService> BuildTestSyncService(
    content::BrowserContext* context) {
  return std::make_unique<syncer::TestSyncService>();
}

}  // namespace

namespace password_manager {

using autofill::test::ServerCacheReplayer;
using autofill::test::ServerUrlLoader;

// Harness for running password manager scenarios on captured real-world sites.
// Test params:
//  - string Recipe: the name of the captured site file and the test recipe
//        file.
class CapturedSitesPasswordManagerBrowserTest
    : public InProcessBrowserTest,
      public captured_sites_test_utils::
          TestRecipeReplayChromeFeatureActionExecutor,
      public ::testing::WithParamInterface<CapturedSiteParams> {
 public:
  CapturedSitesPasswordManagerBrowserTest(
      const CapturedSitesPasswordManagerBrowserTest&) = delete;
  CapturedSitesPasswordManagerBrowserTest& operator=(
      const CapturedSitesPasswordManagerBrowserTest&) = delete;

  // TestRecipeReplayChromeFeatureActionExecutor:
  bool AddCredential(const std::string& origin,
                     const std::string& username,
                     const std::string& password) override {
    scoped_refptr<password_manager::PasswordStoreInterface> password_store =
        PasswordStoreFactory::GetForProfile(browser()->profile(),
                                            ServiceAccessType::EXPLICIT_ACCESS);
    password_manager::PasswordForm signin_form;
    signin_form.url = GURL(origin);
    signin_form.signon_realm = origin;
    signin_form.password_value = base::ASCIIToUTF16(password);
    signin_form.username_value = base::ASCIIToUTF16(username);
    password_store->AddLogin(signin_form);
    return true;
  }

  bool SavePassword() override {
    BubbleObserver bubble_observer(WebContents());
    if (bubble_observer.IsSavePromptAvailable()) {
      bubble_observer.AcceptSavePrompt();
      PasswordManagerBrowserTestBase::WaitForPasswordStore(browser());
      // Hide the Save Password Prompt UI.
      TabDialogs::FromWebContents(WebContents())->HideManagePasswordsBubble();
      content::RunAllPendingInMessageLoop();
      return true;
    }
    ADD_FAILURE() << "No Save Password prompt!";
    return false;
  }

  bool UpdatePassword() override {
    BubbleObserver bubble_observer(WebContents());
    if (bubble_observer.IsUpdatePromptAvailable()) {
      bubble_observer.AcceptUpdatePrompt();
      PasswordManagerBrowserTestBase::WaitForPasswordStore(browser());
      // Hide the Update Password Prompt UI.
      TabDialogs::FromWebContents(WebContents())->HideManagePasswordsBubble();
      content::RunAllPendingInMessageLoop();
      return true;
    }
    ADD_FAILURE() << "No Update Password prompt!";
    return false;
  }

  bool WaitForSaveFallback() override {
    BubbleObserver bubble_observer(WebContents());
    if (bubble_observer.WaitForFallbackForSaving(kWaitForSaveFallbackInterval))
      return true;
    ADD_FAILURE() << "Chrome did not show the save fallback icon!";
    return false;
  }

  bool IsChromeShowingPasswordGenerationPrompt() override {
    return observer_.popup_showing() &&
           observer_.state() ==
               PasswordGenerationPopupController::kOfferGeneration;
  }

  bool HasChromeShownSavePasswordPrompt() override {
    BubbleObserver bubble_observer(WebContents());
    return bubble_observer.IsSavePromptShownAutomatically();
  }

  bool HasChromeStoredCredential(const std::string& origin,
                                 const std::string& username,
                                 const std::string& password) override {
    scoped_refptr<password_manager::PasswordStoreInterface> password_store =
        PasswordStoreFactory::GetForProfile(browser()->profile(),
                                            ServiceAccessType::EXPLICIT_ACCESS);
    FakePasswordStoreBackend* fake_backend =
        static_cast<FakePasswordStoreBackend*>(
            password_store->GetBackendForTesting());

    auto found = fake_backend->stored_passwords().find(origin);
    if (fake_backend->stored_passwords().end() == found) {
      return false;
    }

    const std::vector<password_manager::PasswordForm>& passwords_vector =
        found->second;
    for (const auto& found_password : passwords_vector) {
      if (base::ASCIIToUTF16(username) == found_password.username_value &&
          base::ASCIIToUTF16(password) == found_password.password_value) {
        return true;
      }
    }

    return false;
  }

 protected:
  CapturedSitesPasswordManagerBrowserTest() = default;
  ~CapturedSitesPasswordManagerBrowserTest() override = default;

  // InProcessBrowserTest:
  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating([](content::BrowserContext* context) {
                  // Set up a TestSyncService which will happily return
                  // "everything is active" so that password generation is
                  // considered enabled.
                  SyncServiceFactory::GetInstance()->SetTestingFactory(
                      context, base::BindRepeating(&BuildTestSyncService));

                  PasswordStoreFactory::GetInstance()->SetTestingFactory(
                      context,
                      base::BindRepeating(
                          &password_manager::BuildPasswordStoreWithFakeBackend<
                              content::BrowserContext>));
                }));
  }

  void SetUpOnMainThread() override {
    PasswordManagerBrowserTestBase::GetNewTab(browser(), &web_contents_);
    recipe_replayer_ =
        std::make_unique<captured_sites_test_utils::TestRecipeReplayer>(
            browser(), this);
    recipe_replayer()->Setup();
    SetServerUrlLoader(
        std::make_unique<ServerUrlLoader>(std::make_unique<ServerCacheReplayer>(
            GetParam().capture_file_path,
            ServerCacheReplayer::kOptionFailOnInvalidJsonRecord |
                ServerCacheReplayer::kOptionSplitRequestsByForm)));

    ChromePasswordManagerClient* client =
        ChromePasswordManagerClient::FromWebContents(WebContents());
    client->SetTestObserver(&observer_);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{autofill::features::kAutofillDisplaceRemovedForms,
                              autofill::features::kAutofillShowTypePredictions,
                              features::kUsernameFirstFlow,
                              autofill::features::
                                  kAutofillUseUnassociatedListedElements},
        {});
    command_line->AppendSwitch(autofill::switches::kShowAutofillSignatures);
    captured_sites_test_utils::TestRecipeReplayer::SetUpHostResolverRules(
        command_line);
    captured_sites_test_utils::TestRecipeReplayer::SetUpCommandLine(
        command_line);
  }

  void SetServerUrlLoader(std::unique_ptr<ServerUrlLoader> server_url_loader) {
    server_url_loader_ = std::move(server_url_loader);
  }

  void TearDownOnMainThread() override {
    recipe_replayer()->Cleanup();
    // Need to delete the URL loader and its underlying interceptor on the main
    // thread. Will result in a fatal crash otherwise. The pointer  has its
    // memory cleaned up twice: first time in that single thread, a second time
    // when the fixture's destructor is called, which will have no effect since
    // the raw pointer will be nullptr.
    server_url_loader_.reset(nullptr);
  }

  captured_sites_test_utils::TestRecipeReplayer* recipe_replayer() {
    return recipe_replayer_.get();
  }

  content::WebContents* WebContents() {
    // return web_contents_;
    return web_contents_;
  }

 private:
  TestGenerationPopupObserver observer_;
  std::unique_ptr<captured_sites_test_utils::TestRecipeReplayer>
      recipe_replayer_;
  base::test::ScopedFeatureList feature_list_;
  content::WebContents* web_contents_ = nullptr;
  std::unique_ptr<ServerUrlLoader> server_url_loader_;

  base::CallbackListSubscription create_services_subscription_;
};

IN_PROC_BROWSER_TEST_P(CapturedSitesPasswordManagerBrowserTest, Recipe) {
  captured_sites_test_utils::PrintInstructions(
      "password_manager_captured_sites_interactive_uitest");

  base::FilePath src_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &src_dir));

  bool test_completed = recipe_replayer()->ReplayTest(
      GetParam().capture_file_path, GetParam().recipe_file_path,
      captured_sites_test_utils::GetCommandFilePath());
  if (!test_completed)
    ADD_FAILURE() << "Full execution was unable to complete.";
}

// This test is called with a dynamic list and may be empty during the Autofill
// run instance, so adding GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST a la
// crbug/1192206
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    CapturedSitesPasswordManagerBrowserTest);
INSTANTIATE_TEST_SUITE_P(
    All,
    CapturedSitesPasswordManagerBrowserTest,
    testing::ValuesIn(GetCapturedSites(GetReplayFilesRootDirectory())),
    GetParamAsString());
}  // namespace password_manager
