// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate_impl.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router_factory.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/content/browser/password_manager_log_router_factory.h"
#include "components/password_manager/core/browser/compromised_credentials_table.h"
#include "components/password_manager/core/browser/mock_password_feature_manager.h"
#include "components/password_manager/core/browser/password_list_sorter.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/reauth_purpose.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/test_event_router.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/test/test_clipboard.h"

using MockReauthCallback = base::MockCallback<
    password_manager::PasswordAccessAuthenticator::ReauthCallback>;
using PasswordFormList = std::vector<std::unique_ptr<autofill::PasswordForm>>;
using password_manager::ReauthPurpose;
using password_manager::TestPasswordStore;
using ::testing::Eq;
using ::testing::Ne;
using ::testing::Return;
using ::testing::StrictMock;
namespace extensions {

namespace {

constexpr char kHistogramName[] = "PasswordManager.AccessPasswordInSettings";

using MockPlaintextPasswordCallback =
    base::MockCallback<PasswordsPrivateDelegate::PlaintextPasswordCallback>;

scoped_refptr<TestPasswordStore> CreateAndUseTestPasswordStore(
    Profile* profile) {
  return base::WrapRefCounted(static_cast<TestPasswordStore*>(
      PasswordStoreFactory::GetInstance()
          ->SetTestingFactoryAndUse(
              profile,
              base::BindRepeating(&password_manager::BuildPasswordStore<
                                  content::BrowserContext, TestPasswordStore>))
          .get()));
}

class MockPasswordManagerClient : public ChromePasswordManagerClient {
 public:
  // Creates the mock and attaches it to |web_contents|.
  static MockPasswordManagerClient* CreateForWebContentsAndGet(
      content::WebContents* web_contents);

  ~MockPasswordManagerClient() override = default;

  // ChromePasswordManagerClient overrides.
  MOCK_METHOD(void,
              TriggerReauthForPrimaryAccount,
              (base::OnceCallback<void(ReauthSucceeded)>),
              (override));
  const password_manager::MockPasswordFeatureManager*
  GetPasswordFeatureManager() const override {
    return &mock_password_feature_manager_;
  }

  password_manager::MockPasswordFeatureManager* GetPasswordFeatureManager() {
    return &mock_password_feature_manager_;
  }

 private:
  explicit MockPasswordManagerClient(content::WebContents* web_contents)
      : ChromePasswordManagerClient(web_contents, nullptr) {}

  password_manager::MockPasswordFeatureManager mock_password_feature_manager_;
};

// static
MockPasswordManagerClient*
MockPasswordManagerClient::CreateForWebContentsAndGet(
    content::WebContents* web_contents) {
  // Avoid creation of log router.
  password_manager::PasswordManagerLogRouterFactory::GetInstance()
      ->SetTestingFactory(
          web_contents->GetBrowserContext(),
          base::BindRepeating(
              [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
                return nullptr;
              }));
  auto* mock_client = new MockPasswordManagerClient(web_contents);
  web_contents->SetUserData(UserDataKey(), base::WrapUnique(mock_client));
  return mock_client;
}

class PasswordEventObserver
    : public extensions::TestEventRouter::EventObserver {
 public:
  // The observer will only listen to events with the |event_name|.
  explicit PasswordEventObserver(const std::string& event_name);

  ~PasswordEventObserver() override;

  // Removes |event_args_| from |*this| and returns them.
  base::Value PassEventArgs();

  // extensions::TestEventRouter::EventObserver:
  void OnBroadcastEvent(const extensions::Event& event) override;

 private:
  // The name of the observed event.
  const std::string event_name_;

  // The arguments passed for the last observed event.
  base::Value event_args_;

  DISALLOW_COPY_AND_ASSIGN(PasswordEventObserver);
};

PasswordEventObserver::PasswordEventObserver(const std::string& event_name)
    : event_name_(event_name) {}

PasswordEventObserver::~PasswordEventObserver() = default;

base::Value PasswordEventObserver::PassEventArgs() {
  return std::move(event_args_);
}

void PasswordEventObserver::OnBroadcastEvent(const extensions::Event& event) {
  if (event.event_name != event_name_) {
    return;
  }
  event_args_ = event.event_args->Clone();
}

std::unique_ptr<KeyedService> BuildPasswordsPrivateEventRouter(
    content::BrowserContext* context) {
  return std::unique_ptr<KeyedService>(
      PasswordsPrivateEventRouter::Create(context));
}

autofill::PasswordForm CreateSampleForm() {
  autofill::PasswordForm form;
  form.signon_realm = "http://abc1.com";
  form.origin = GURL("http://abc1.com");
  form.username_value = base::ASCIIToUTF16("test@gmail.com");
  form.password_value = base::ASCIIToUTF16("test");
  return form;
}

}  // namespace

class PasswordsPrivateDelegateImplTest : public testing::Test {
 public:
  PasswordsPrivateDelegateImplTest();
  ~PasswordsPrivateDelegateImplTest() override;

  // Sets up a testing password store and fills it with |forms|.
  void SetUpPasswordStore(std::vector<autofill::PasswordForm> forms);

  // Sets up a testing EventRouter with a production
  // PasswordsPrivateEventRouter.
  void SetUpRouters();

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  extensions::TestEventRouter* event_router_ = nullptr;
  scoped_refptr<TestPasswordStore> store_ =
      CreateAndUseTestPasswordStore(&profile_);
  ui::TestClipboard* test_clipboard_ =
      ui::TestClipboard::CreateForCurrentThread();

 private:
  base::HistogramTester histogram_tester_;
  DISALLOW_COPY_AND_ASSIGN(PasswordsPrivateDelegateImplTest);
};

PasswordsPrivateDelegateImplTest::PasswordsPrivateDelegateImplTest() {
  SetUpRouters();
}

PasswordsPrivateDelegateImplTest::~PasswordsPrivateDelegateImplTest() {
  ui::Clipboard::DestroyClipboardForCurrentThread();
}

void PasswordsPrivateDelegateImplTest::SetUpPasswordStore(
    std::vector<autofill::PasswordForm> forms) {
  for (const autofill::PasswordForm& form : forms) {
    store_->AddLogin(form);
  }
  // Spin the loop to allow PasswordStore tasks being processed.
  base::RunLoop().RunUntilIdle();
}

void PasswordsPrivateDelegateImplTest::SetUpRouters() {
  event_router_ = extensions::CreateAndUseTestEventRouter(&profile_);
  // Set the production PasswordsPrivateEventRouter::Create as a testing
  // factory, because at some point during the preceding initialization, a null
  // factory is set, resulting in nul PasswordsPrivateEventRouter.
  PasswordsPrivateEventRouterFactory::GetInstance()->SetTestingFactory(
      &profile_, base::BindRepeating(&BuildPasswordsPrivateEventRouter));
}

TEST_F(PasswordsPrivateDelegateImplTest, GetSavedPasswordsList) {
  PasswordsPrivateDelegateImpl delegate(&profile_);

  base::MockCallback<PasswordsPrivateDelegate::UiEntriesCallback> callback;
  EXPECT_CALL(callback, Run).Times(0);
  delegate.GetSavedPasswordsList(callback.Get());

  PasswordFormList list;
  list.push_back(std::make_unique<autofill::PasswordForm>());

  EXPECT_CALL(callback, Run);
  delegate.SetPasswordList(list);

  EXPECT_CALL(callback, Run);
  delegate.GetSavedPasswordsList(callback.Get());
}

TEST_F(PasswordsPrivateDelegateImplTest, GetPasswordExceptionsList) {
  PasswordsPrivateDelegateImpl delegate(&profile_);

  base::MockCallback<PasswordsPrivateDelegate::ExceptionEntriesCallback>
      callback;
  EXPECT_CALL(callback, Run).Times(0);
  delegate.GetPasswordExceptionsList(callback.Get());

  PasswordFormList list;
  list.push_back(std::make_unique<autofill::PasswordForm>());

  EXPECT_CALL(callback, Run);
  delegate.SetPasswordExceptionList(list);

  EXPECT_CALL(callback, Run);
  delegate.GetPasswordExceptionsList(callback.Get());
}

TEST_F(PasswordsPrivateDelegateImplTest, ChangeSavedPassword) {
  autofill::PasswordForm sample_form = CreateSampleForm();
  SetUpPasswordStore({sample_form});

  PasswordsPrivateDelegateImpl delegate(&profile_);
  // Spin the loop to allow PasswordStore tasks posted on the creation of
  // |delegate| to be completed.
  base::RunLoop().RunUntilIdle();

  // Double check that the contents of the passwords list matches our
  // expectation.
  bool got_passwords = false;
  delegate.GetSavedPasswordsList(base::BindLambdaForTesting(
      [&](const PasswordsPrivateDelegate::UiEntries& password_list) {
        got_passwords = true;
        ASSERT_EQ(1u, password_list.size());
        EXPECT_EQ(sample_form.username_value,
                  base::UTF8ToUTF16(password_list[0].username));
      }));
  EXPECT_TRUE(got_passwords);

  int sample_form_id = delegate.GetPasswordIdGeneratorForTesting().GenerateId(
      password_manager::CreateSortKey(sample_form));
  delegate.ChangeSavedPassword(sample_form_id, base::ASCIIToUTF16("new_user"),
                               base::ASCIIToUTF16("new_pass"));

  // Spin the loop to allow PasswordStore tasks posted when changing the
  // password to be completed.
  base::RunLoop().RunUntilIdle();

  // Check that the changing the password got reflected in the passwords list.
  got_passwords = false;
  delegate.GetSavedPasswordsList(base::BindLambdaForTesting(
      [&](const PasswordsPrivateDelegate::UiEntries& password_list) {
        got_passwords = true;
        ASSERT_EQ(1u, password_list.size());
        EXPECT_EQ(base::ASCIIToUTF16("new_user"),
                  base::UTF8ToUTF16(password_list[0].username));
      }));
  EXPECT_TRUE(got_passwords);
}

// Checking callback result of RequestPlaintextPassword with reason Copy.
// By implementation for Copy, callback will receive empty string.
TEST_F(PasswordsPrivateDelegateImplTest, TestCopyPasswordCallbackResult) {
  autofill::PasswordForm form = CreateSampleForm();
  SetUpPasswordStore({form});

  PasswordsPrivateDelegateImpl delegate(&profile_);
  base::RunLoop().RunUntilIdle();

  MockReauthCallback callback;
  delegate.set_os_reauth_call(callback.Get());

  EXPECT_CALL(callback, Run(ReauthPurpose::COPY_PASSWORD))
      .WillOnce(Return(true));

  MockPlaintextPasswordCallback password_callback;
  EXPECT_CALL(password_callback, Run(Eq(base::string16())));
  delegate.RequestPlaintextPassword(
      0, api::passwords_private::PLAINTEXT_REASON_COPY, password_callback.Get(),
      nullptr);

  base::string16 result;
  test_clipboard_->ReadText(ui::ClipboardBuffer::kCopyPaste, &result);
  EXPECT_EQ(form.password_value, result);

  histogram_tester().ExpectUniqueSample(
      kHistogramName, password_manager::metrics_util::ACCESS_PASSWORD_COPIED,
      1);
}

TEST_F(PasswordsPrivateDelegateImplTest, TestShouldReauthForOptIn) {
  // This enables uses of TestWebContents.
  content::RenderViewHostTestEnabler test_render_host_factories;

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
  auto* client =
      MockPasswordManagerClient::CreateForWebContentsAndGet(web_contents.get());
  ON_CALL(*(client->GetPasswordFeatureManager()), IsOptedInForAccountStorage)
      .WillByDefault(Return(false));

  EXPECT_CALL(*client, TriggerReauthForPrimaryAccount);

  PasswordsPrivateDelegateImpl delegate(&profile_);
  delegate.SetAccountStorageOptIn(true, web_contents.get());
}

TEST_F(PasswordsPrivateDelegateImplTest,
       TestShouldNotReauthForOptOutAndShouldSetPref) {
  // This enables uses of TestWebContents.
  content::RenderViewHostTestEnabler test_render_host_factories;

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
  auto* client =
      MockPasswordManagerClient::CreateForWebContentsAndGet(web_contents.get());
  password_manager::MockPasswordFeatureManager* feature_manager =
      client->GetPasswordFeatureManager();
  ON_CALL(*feature_manager, IsOptedInForAccountStorage)
      .WillByDefault(Return(true));

  EXPECT_CALL(*client, TriggerReauthForPrimaryAccount).Times(0);
  EXPECT_CALL(*feature_manager, OptOutOfAccountStorageAndClearSettings);

  PasswordsPrivateDelegateImpl delegate(&profile_);
  delegate.SetAccountStorageOptIn(false, web_contents.get());
}

TEST_F(PasswordsPrivateDelegateImplTest, TestCopyPasswordCallbackResultFail) {
  SetUpPasswordStore({CreateSampleForm()});

  PasswordsPrivateDelegateImpl delegate(&profile_);
  base::RunLoop().RunUntilIdle();

  MockReauthCallback callback;
  delegate.set_os_reauth_call(callback.Get());

  EXPECT_CALL(callback, Run(ReauthPurpose::COPY_PASSWORD))
      .WillOnce(Return(false));

  base::Time before_call = test_clipboard_->GetLastModifiedTime();

  MockPlaintextPasswordCallback password_callback;
  EXPECT_CALL(password_callback, Run(Eq(base::nullopt)));
  delegate.RequestPlaintextPassword(
      0, api::passwords_private::PLAINTEXT_REASON_COPY, password_callback.Get(),
      nullptr);
  // Clipboard should not be modifiend in case Reauth failed
  base::string16 result;
  test_clipboard_->ReadText(ui::ClipboardBuffer::kCopyPaste, &result);
  EXPECT_EQ(base::string16(), result);
  EXPECT_EQ(before_call, test_clipboard_->GetLastModifiedTime());

  // Since Reauth had failed password was not copied and metric wasn't recorded
  histogram_tester().ExpectTotalCount(kHistogramName, 0);
}

TEST_F(PasswordsPrivateDelegateImplTest, TestPassedReauthOnView) {
  SetUpPasswordStore({CreateSampleForm()});

  PasswordsPrivateDelegateImpl delegate(&profile_);
  // Spin the loop to allow PasswordStore tasks posted on the creation of
  // |delegate| to be completed.
  base::RunLoop().RunUntilIdle();

  MockReauthCallback callback;
  delegate.set_os_reauth_call(callback.Get());

  EXPECT_CALL(callback, Run(ReauthPurpose::VIEW_PASSWORD))
      .WillOnce(Return(true));

  MockPlaintextPasswordCallback password_callback;
  EXPECT_CALL(password_callback, Run(Eq(base::ASCIIToUTF16("test"))));
  delegate.RequestPlaintextPassword(
      0, api::passwords_private::PLAINTEXT_REASON_VIEW, password_callback.Get(),
      nullptr);

  histogram_tester().ExpectUniqueSample(
      kHistogramName, password_manager::metrics_util::ACCESS_PASSWORD_VIEWED,
      1);
}

TEST_F(PasswordsPrivateDelegateImplTest, TestFailedReauthOnView) {
  SetUpPasswordStore({CreateSampleForm()});

  PasswordsPrivateDelegateImpl delegate(&profile_);
  // Spin the loop to allow PasswordStore tasks posted on the creation of
  // |delegate| to be completed.
  base::RunLoop().RunUntilIdle();

  MockReauthCallback callback;
  delegate.set_os_reauth_call(callback.Get());

  EXPECT_CALL(callback, Run(ReauthPurpose::VIEW_PASSWORD))
      .WillOnce(Return(false));

  MockPlaintextPasswordCallback password_callback;
  EXPECT_CALL(password_callback, Run(Eq(base::nullopt)));
  delegate.RequestPlaintextPassword(
      0, api::passwords_private::PLAINTEXT_REASON_VIEW, password_callback.Get(),
      nullptr);

  // Since Reauth had failed password was not viewed and metric wasn't recorded
  histogram_tester().ExpectTotalCount(kHistogramName, 0);
}

TEST_F(PasswordsPrivateDelegateImplTest, TestReauthOnExport) {
  SetUpPasswordStore({CreateSampleForm()});
  StrictMock<base::MockCallback<base::OnceCallback<void(const std::string&)>>>
      mock_accepted;

  PasswordsPrivateDelegateImpl delegate(&profile_);
  // Spin the loop to allow PasswordStore tasks posted on the creation of
  // |delegate| to be completed.
  base::RunLoop().RunUntilIdle();

  MockReauthCallback callback;
  delegate.set_os_reauth_call(callback.Get());

  EXPECT_CALL(mock_accepted, Run(std::string())).Times(2);

  EXPECT_CALL(callback, Run(ReauthPurpose::EXPORT)).WillOnce(Return(true));
  delegate.ExportPasswords(mock_accepted.Get(), nullptr);

  // Export should ignore previous reauthentication results.
  EXPECT_CALL(callback, Run(ReauthPurpose::EXPORT)).WillOnce(Return(true));
  delegate.ExportPasswords(mock_accepted.Get(), nullptr);
}

TEST_F(PasswordsPrivateDelegateImplTest, TestReauthFailedOnExport) {
  SetUpPasswordStore({CreateSampleForm()});
  StrictMock<base::MockCallback<base::OnceCallback<void(const std::string&)>>>
      mock_accepted;

  PasswordsPrivateDelegateImpl delegate(&profile_);
  // Spin the loop to allow PasswordStore tasks posted on the creation of
  // |delegate| to be completed.
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_accepted, Run(std::string("reauth-failed")));

  MockReauthCallback callback;
  delegate.set_os_reauth_call(callback.Get());

  EXPECT_CALL(callback, Run(ReauthPurpose::EXPORT)).WillOnce(Return(false));
  delegate.ExportPasswords(mock_accepted.Get(), nullptr);
}

// Verifies that PasswordsPrivateDelegateImpl::GetPlaintextCompromisedPassword
// fails if the re-auth fails.
TEST_F(PasswordsPrivateDelegateImplTest,
       TestReauthOnGetPlaintextCompromisedPasswordFails) {
  PasswordsPrivateDelegateImpl delegate(&profile_);

  MockReauthCallback reauth_callback;
  delegate.set_os_reauth_call(reauth_callback.Get());

  base::MockCallback<
      PasswordsPrivateDelegate::PlaintextCompromisedPasswordCallback>
      credential_callback;

  EXPECT_CALL(reauth_callback, Run(ReauthPurpose::VIEW_PASSWORD))
      .WillOnce(Return(false));
  EXPECT_CALL(credential_callback, Run(Eq(base::nullopt)));

  delegate.GetPlaintextCompromisedPassword(
      api::passwords_private::CompromisedCredential(),
      api::passwords_private::PLAINTEXT_REASON_VIEW, nullptr,
      credential_callback.Get());
}

// Verifies that PasswordsPrivateDelegateImpl::GetPlaintextCompromisedPassword
// succeeds if the re-auth succeeds and there is a matching compromised
// credential in the store.
TEST_F(PasswordsPrivateDelegateImplTest,
       TestReauthOnGetPlaintextCompromisedPassword) {
  PasswordsPrivateDelegateImpl delegate(&profile_);

  autofill::PasswordForm form = CreateSampleForm();
  password_manager::CompromisedCredentials compromised_credentials;
  compromised_credentials.signon_realm = form.signon_realm;
  compromised_credentials.username = form.username_value;
  store_->AddLogin(form);
  store_->AddCompromisedCredentials(compromised_credentials);
  base::RunLoop().RunUntilIdle();

  api::passwords_private::CompromisedCredential credential =
      std::move(delegate.GetCompromisedCredentials().at(0));

  MockReauthCallback reauth_callback;
  delegate.set_os_reauth_call(reauth_callback.Get());

  base::MockCallback<
      PasswordsPrivateDelegate::PlaintextCompromisedPasswordCallback>
      credential_callback;

  base::Optional<api::passwords_private::CompromisedCredential> opt_credential;
  EXPECT_CALL(reauth_callback, Run(ReauthPurpose::VIEW_PASSWORD))
      .WillOnce(Return(true));
  EXPECT_CALL(credential_callback, Run).WillOnce(MoveArg(&opt_credential));

  delegate.GetPlaintextCompromisedPassword(
      std::move(credential), api::passwords_private::PLAINTEXT_REASON_VIEW,
      nullptr, credential_callback.Get());

  ASSERT_TRUE(opt_credential.has_value());
  EXPECT_EQ(form.signon_realm, opt_credential->signon_realm);
  EXPECT_EQ(form.username_value, base::UTF8ToUTF16(opt_credential->username));
  EXPECT_EQ(form.password_value, base::UTF8ToUTF16(*opt_credential->password));
}

}  // namespace extensions
