// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/save_update_password_message_delegate.h"

#include "base/android/jni_android.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/messages/android/mock_message_dispatcher_bridge.h"
#include "components/password_manager/core/browser/mock_password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using password_manager::MockPasswordFormManagerForUI;
using password_manager::PasswordForm;
using password_manager::PasswordFormManagerForUI;
using password_manager::PasswordFormMetricsRecorder;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Return;
using ::testing::ReturnRef;

namespace {
constexpr char kDefaultUrl[] = "http://example.com";
constexpr char16_t kOrigin[] = u"example.com";
constexpr char16_t kUsername[] = u"username";
constexpr char16_t kUsername2[] = u"username2";
constexpr char16_t kPassword[] = u"password";
constexpr char kAccountEmail[] = "account@example.com";
constexpr char16_t kAccountEmail16[] = u"account@example.com";
constexpr char kSaveUIDismissalReasonHistogramName[] =
    "PasswordManager.SaveUIDismissalReason";
constexpr char kUpdateUIDismissalReasonHistogramName[] =
    "PasswordManager.UpdateUIDismissalReason";
}  // namespace

class MockPasswordEditDialog : public PasswordEditDialog {
 public:
  MOCK_METHOD(void,
              Show,
              (const std::vector<std::u16string>& usernames,
               int selected_username_index,
               const std::u16string& password,
               const std::u16string& origin,
               const std::string& account_email),
              (override));
  MOCK_METHOD(void, Dismiss, (), (override));
};

class SaveUpdatePasswordMessageDelegateTest
    : public ChromeRenderViewHostTestHarness {
 public:
  SaveUpdatePasswordMessageDelegateTest();

 protected:
  void SetUp() override;
  void TearDown() override;

  std::unique_ptr<MockPasswordFormManagerForUI> CreateFormManager(
      const GURL& password_form_url,
      const std::vector<const PasswordForm*>* best_matches);
  void SetPendingCredentials(std::u16string username, std::u16string password);
  static PasswordForm CreatePasswordForm(std::u16string username,
                                         std::u16string password);

  void EnqueueMessage(std::unique_ptr<PasswordFormManagerForUI> form_to_save,
                      bool user_signed_in,
                      bool update_password);
  void TriggerActionClick();
  void TriggerPasswordEditDialog();

  void ExpectDismissMessageCall();
  void DismissMessage(messages::DismissReason dismiss_reason);
  void DestroyDelegate();

  messages::MessageWrapper* GetMessageWrapper();

  // Password edit dialog factory function that is passed to
  // SaveUpdatePasswordMessageDelegate. Passes the dialog prepared by
  // PreparePasswordEditDialog. Captures accept and dismiss callbacks.
  std::unique_ptr<PasswordEditDialog> CreatePasswordEditDialog(
      content::WebContents* web_contents,
      PasswordEditDialog::DialogAcceptedCallback dialog_accepted_callback,
      PasswordEditDialog::DialogDismissedCallback dialog_dismissed_callback);

  // Creates a mock of PasswordEditDialog that will be passed to
  // SaveUpdatePasswordMessageDelegate through CreatePasswordEditDialog factory.
  // Returns non-owning pointer to the mock for test to configure mock
  // expectations.
  MockPasswordEditDialog* PreparePasswordEditDialog();

  void TriggerDialogAcceptedCallback(int selected_user_index);
  void TriggerDialogDismissedCallback(bool dialog_accepted);

  void CommitPasswordFormMetrics();
  void VerifyUkmMetrics(const ukm::TestUkmRecorder& ukm_recorder,
                        PasswordFormMetricsRecorder::BubbleDismissalReason
                            expected_dismissal_reason);

  messages::MockMessageDispatcherBridge* message_dispatcher_bridge() {
    return &message_dispatcher_bridge_;
  }

  const std::vector<const PasswordForm*>* empty_best_matches() {
    return &kEmptyBestMatches;
  }

  const std::vector<const PasswordForm*>* two_forms_best_matches() {
    return &kTwoFormsBestMatches;
  }

 private:
  const PasswordForm kPasswordForm1 = CreatePasswordForm(kUsername, kPassword);
  const PasswordForm kPasswordForm2 = CreatePasswordForm(kUsername2, kPassword);
  const std::vector<const PasswordForm*> kEmptyBestMatches = {};
  const std::vector<const PasswordForm*> kTwoFormsBestMatches = {
      &kPasswordForm1, &kPasswordForm2};

  PasswordForm pending_credentials_;
  std::unique_ptr<SaveUpdatePasswordMessageDelegate> delegate_;
  GURL password_form_url_;
  scoped_refptr<PasswordFormMetricsRecorder> metrics_recorder_;
  ukm::SourceId ukm_source_id_;
  messages::MockMessageDispatcherBridge message_dispatcher_bridge_;
  std::unique_ptr<MockPasswordEditDialog> mock_password_edit_dialog_;
  PasswordEditDialog::DialogAcceptedCallback dialog_accepted_callback_;
  PasswordEditDialog::DialogDismissedCallback dialog_dismissed_callback_;
};

SaveUpdatePasswordMessageDelegateTest::SaveUpdatePasswordMessageDelegateTest()
    : delegate_(base::WrapUnique(
          new SaveUpdatePasswordMessageDelegate(base::BindRepeating(
              &SaveUpdatePasswordMessageDelegateTest::CreatePasswordEditDialog,
              base::Unretained(this))))) {}

void SaveUpdatePasswordMessageDelegateTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  ChromePasswordManagerClient::CreateForWebContentsWithAutofillClient(
      web_contents(), nullptr);
  ukm_source_id_ = ukm::UkmRecorder::GetNewSourceID();
  metrics_recorder_ = base::MakeRefCounted<PasswordFormMetricsRecorder>(
      true /*is_main_frame_secure*/, ukm_source_id_, nullptr /*pref_service*/);
  NavigateAndCommit(GURL(kDefaultUrl));

  messages::MessageDispatcherBridge::SetInstanceForTesting(
      &message_dispatcher_bridge_);
}

void SaveUpdatePasswordMessageDelegateTest::TearDown() {
  messages::MessageDispatcherBridge::SetInstanceForTesting(nullptr);
  ChromeRenderViewHostTestHarness::TearDown();
}

std::unique_ptr<MockPasswordFormManagerForUI>
SaveUpdatePasswordMessageDelegateTest::CreateFormManager(
    const GURL& password_form_url,
    const std::vector<const PasswordForm*>* best_matches) {
  password_form_url_ = password_form_url;
  auto form_manager =
      std::make_unique<testing::NiceMock<MockPasswordFormManagerForUI>>();
  ON_CALL(*form_manager, GetPendingCredentials())
      .WillByDefault(ReturnRef(pending_credentials_));
  ON_CALL(*form_manager, GetCredentialSource())
      .WillByDefault(Return(password_manager::metrics_util::
                                CredentialSourceType::kPasswordManager));
  ON_CALL(*form_manager, GetURL()).WillByDefault(ReturnRef(password_form_url_));
  ON_CALL(*form_manager, GetBestMatches())
      .WillByDefault(ReturnRef(*best_matches));
  ON_CALL(*form_manager, GetFederatedMatches())
      .WillByDefault(Return(std::vector<const PasswordForm*>{}));

  ON_CALL(*form_manager, GetMetricsRecorder())
      .WillByDefault(Return(metrics_recorder_.get()));
  return form_manager;
}

void SaveUpdatePasswordMessageDelegateTest::SetPendingCredentials(
    std::u16string username,
    std::u16string password) {
  pending_credentials_.username_value = std::move(username);
  pending_credentials_.password_value = std::move(password);
}

// static
PasswordForm SaveUpdatePasswordMessageDelegateTest::CreatePasswordForm(
    std::u16string username,
    std::u16string password) {
  PasswordForm password_form;
  password_form.username_value = std::move(username);
  password_form.password_value = std::move(password);
  return password_form;
}

void SaveUpdatePasswordMessageDelegateTest::EnqueueMessage(
    std::unique_ptr<PasswordFormManagerForUI> form_to_save,
    bool user_signed_in,
    bool update_password) {
  absl::optional<AccountInfo> account_info;
  if (user_signed_in) {
    account_info = AccountInfo();
    account_info.value().email = kAccountEmail;
  }
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage);
  delegate_->DisplaySaveUpdatePasswordPromptInternal(
      web_contents(), std::move(form_to_save), account_info, update_password);
}

void SaveUpdatePasswordMessageDelegateTest::TriggerActionClick() {
  GetMessageWrapper()->HandleActionClick(base::android::AttachCurrentThread());
  // Simulate call from Java to dismiss message on primary button click.
  DismissMessage(messages::DismissReason::PRIMARY_ACTION);
}

void SaveUpdatePasswordMessageDelegateTest::TriggerPasswordEditDialog() {
  GetMessageWrapper()->HandleSecondaryActionClick(
      base::android::AttachCurrentThread());
}

void SaveUpdatePasswordMessageDelegateTest::ExpectDismissMessageCall() {
  EXPECT_CALL(message_dispatcher_bridge_, DismissMessage)
      .WillOnce([](messages::MessageWrapper* message,
                   messages::DismissReason dismiss_reason) {
        message->HandleDismissCallback(base::android::AttachCurrentThread(),
                                       static_cast<int>(dismiss_reason));
      });
}

void SaveUpdatePasswordMessageDelegateTest::DismissMessage(
    messages::DismissReason dismiss_reason) {
  ExpectDismissMessageCall();
  delegate_->DismissSaveUpdatePasswordMessage(dismiss_reason);
  EXPECT_EQ(nullptr, GetMessageWrapper());
}

void SaveUpdatePasswordMessageDelegateTest::DestroyDelegate() {
  delegate_.reset();
}

messages::MessageWrapper*
SaveUpdatePasswordMessageDelegateTest::GetMessageWrapper() {
  return delegate_->message_.get();
}

std::unique_ptr<PasswordEditDialog>
SaveUpdatePasswordMessageDelegateTest::CreatePasswordEditDialog(
    content::WebContents* web_contents,
    PasswordEditDialog::DialogAcceptedCallback dialog_accepted_callback,
    PasswordEditDialog::DialogDismissedCallback dialog_dismissed_callback) {
  dialog_accepted_callback_ = std::move(dialog_accepted_callback);
  dialog_dismissed_callback_ = std::move(dialog_dismissed_callback);
  return std::move(mock_password_edit_dialog_);
}

MockPasswordEditDialog*
SaveUpdatePasswordMessageDelegateTest::PreparePasswordEditDialog() {
  mock_password_edit_dialog_ = std::make_unique<MockPasswordEditDialog>();
  return mock_password_edit_dialog_.get();
}

void SaveUpdatePasswordMessageDelegateTest::TriggerDialogAcceptedCallback(
    int selected_username_index) {
  std::move(dialog_accepted_callback_).Run(selected_username_index);
}

void SaveUpdatePasswordMessageDelegateTest::TriggerDialogDismissedCallback(
    bool dialog_accepted) {
  std::move(dialog_dismissed_callback_).Run(dialog_accepted);
}
void SaveUpdatePasswordMessageDelegateTest::CommitPasswordFormMetrics() {
  // PasswordFormMetricsRecorder::dtor commits accumulated metrics.
  metrics_recorder_.reset();
}

void SaveUpdatePasswordMessageDelegateTest::VerifyUkmMetrics(
    const ukm::TestUkmRecorder& ukm_recorder,
    PasswordFormMetricsRecorder::BubbleDismissalReason
        expected_dismissal_reason) {
  const auto& entries =
      ukm_recorder.GetEntriesByName(ukm::builders::PasswordForm::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const auto* entry : entries) {
    EXPECT_EQ(ukm_source_id_, entry->source_id);
    ukm_recorder.ExpectEntryMetric(
        entry, ukm::builders::PasswordForm::kSaving_Prompt_ShownName, 1);
    ukm_recorder.ExpectEntryMetric(
        entry, ukm::builders::PasswordForm::kSaving_Prompt_TriggerName,
        static_cast<int64_t>(PasswordFormMetricsRecorder::BubbleTrigger::
                                 kPasswordManagerSuggestionAutomatic));
    ukm_recorder.ExpectEntryMetric(
        entry, ukm::builders::PasswordForm::kSaving_Prompt_InteractionName,
        static_cast<int64_t>(expected_dismissal_reason));
  }
}

// Tests that message properties (title, description, icon, button text) are
// set correctly for save password message.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       MessagePropertyValues_SavePassword) {
  SetPendingCredentials(kUsername, kPassword);
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/false);

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SAVE_PASSWORD),
            GetMessageWrapper()->GetTitle());
  EXPECT_NE(std::u16string::npos,
            GetMessageWrapper()->GetDescription().find(kUsername));
  EXPECT_EQ(std::u16string::npos,
            GetMessageWrapper()->GetDescription().find(kPassword));
  EXPECT_EQ(std::u16string::npos,
            GetMessageWrapper()->GetDescription().find(kAccountEmail16));

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_SAVE_BUTTON),
            GetMessageWrapper()->GetPrimaryButtonText());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_BLOCKLIST_BUTTON),
            GetMessageWrapper()->GetSecondaryButtonMenuText());
  EXPECT_EQ(
      ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_INFOBAR_SAVE_PASSWORD),
      GetMessageWrapper()->GetIconResourceId());
  EXPECT_EQ(ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_MESSAGE_SETTINGS),
            GetMessageWrapper()->GetSecondaryIconResourceId());

  DismissMessage(messages::DismissReason::UNKNOWN);
}

// Tests that message properties (title, description, icon, button text) are
// set correctly for update password message.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       MessagePropertyValues_UpdatePassword) {
  SetPendingCredentials(kUsername, kPassword);
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/true);

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_UPDATE_PASSWORD),
            GetMessageWrapper()->GetTitle());

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_UPDATE_BUTTON),
            GetMessageWrapper()->GetPrimaryButtonText());
  EXPECT_EQ(std::u16string(),
            GetMessageWrapper()->GetSecondaryButtonMenuText());

  DismissMessage(messages::DismissReason::UNKNOWN);
}

// Tests that the description is set correctly when signed-in user saves a
// password.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       SignedInDescription_SavePassword) {
  SetPendingCredentials(kUsername, kPassword);
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/false);

  EXPECT_EQ(std::u16string::npos,
            GetMessageWrapper()->GetDescription().find(kUsername));
  EXPECT_EQ(std::u16string::npos,
            GetMessageWrapper()->GetDescription().find(kPassword));
  EXPECT_NE(std::u16string::npos,
            GetMessageWrapper()->GetDescription().find(kAccountEmail16));

  DismissMessage(messages::DismissReason::UNKNOWN);
}

// Tests that the description is set correctly when signed-in user updates a
// password.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       SignedInDescription_UpdatePassword) {
  SetPendingCredentials(kUsername, kPassword);
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/true);

  EXPECT_EQ(std::u16string::npos,
            GetMessageWrapper()->GetDescription().find(kUsername));
  EXPECT_EQ(std::u16string::npos,
            GetMessageWrapper()->GetDescription().find(kPassword));
  EXPECT_NE(std::u16string::npos,
            GetMessageWrapper()->GetDescription().find(kAccountEmail16));

  DismissMessage(messages::DismissReason::UNKNOWN);
}

// Tests that the previous prompt gets dismissed when the new one is enqueued.
TEST_F(SaveUpdatePasswordMessageDelegateTest, OnlyOnePromptAtATime) {
  SetPendingCredentials(kUsername, kPassword);
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/false);

  ExpectDismissMessageCall();
  auto form_manager2 =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EnqueueMessage(std::move(form_manager2), /*user_signed_in=*/true,
                 /*update_password=*/false);
  DismissMessage(messages::DismissReason::UNKNOWN);
}

// Tests that password form is saved and metrics recorded correctly when the
// user clicks "Save" button.
TEST_F(SaveUpdatePasswordMessageDelegateTest, SaveOnActionClick) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EXPECT_CALL(*form_manager, Save());
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());
  TriggerActionClick();
  EXPECT_EQ(nullptr, GetMessageWrapper());

  CommitPasswordFormMetrics();
  VerifyUkmMetrics(
      test_ukm_recorder,
      PasswordFormMetricsRecorder::BubbleDismissalReason::kAccepted);
  histogram_tester.ExpectUniqueSample(
      kSaveUIDismissalReasonHistogramName,
      password_manager::metrics_util::CLICKED_ACCEPT, 1);
}

// Tests that password form is not saved and metrics recorded correctly when the
// user dismisses the message.
TEST_F(SaveUpdatePasswordMessageDelegateTest, DontSaveOnDismiss) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EXPECT_CALL(*form_manager, Save()).Times(0);
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());
  DismissMessage(messages::DismissReason::GESTURE);
  EXPECT_EQ(nullptr, GetMessageWrapper());

  CommitPasswordFormMetrics();
  VerifyUkmMetrics(
      test_ukm_recorder,
      PasswordFormMetricsRecorder::BubbleDismissalReason::kDeclined);
  histogram_tester.ExpectUniqueSample(
      kSaveUIDismissalReasonHistogramName,
      password_manager::metrics_util::CLICKED_CANCEL, 1);
}

// Tests that password form is not saved and metrics recorded correctly when the
// message is autodismissed.
TEST_F(SaveUpdatePasswordMessageDelegateTest, MetricOnAutodismissTimer) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EXPECT_CALL(*form_manager, Save()).Times(0);
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());
  DismissMessage(messages::DismissReason::TIMER);
  EXPECT_EQ(nullptr, GetMessageWrapper());

  CommitPasswordFormMetrics();
  VerifyUkmMetrics(
      test_ukm_recorder,
      PasswordFormMetricsRecorder::BubbleDismissalReason::kIgnored);
  histogram_tester.ExpectUniqueSample(
      kSaveUIDismissalReasonHistogramName,
      password_manager::metrics_util::NO_DIRECT_INTERACTION, 1);
}

// Tests that update password message with a single PasswordForm immediately
// saves the form on Update button tap and doesn't display confirmation dialog.
TEST_F(SaveUpdatePasswordMessageDelegateTest, UpdatePasswordWithSingleForm) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  SetPendingCredentials(kUsername, kPassword);
  PasswordForm password_form = CreatePasswordForm(kUsername, kPassword);
  std::vector<const PasswordForm*> single_form_best_matches = {&password_form};
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), &single_form_best_matches);
  EXPECT_CALL(*form_manager, Save());
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/true);
  EXPECT_NE(nullptr, GetMessageWrapper());
  TriggerActionClick();
  EXPECT_EQ(nullptr, GetMessageWrapper());

  CommitPasswordFormMetrics();
  VerifyUkmMetrics(
      test_ukm_recorder,
      PasswordFormMetricsRecorder::BubbleDismissalReason::kAccepted);
  histogram_tester.ExpectUniqueSample(
      kUpdateUIDismissalReasonHistogramName,
      password_manager::metrics_util::CLICKED_ACCEPT, 1);
}

TEST_F(SaveUpdatePasswordMessageDelegateTest, DialogProperties) {
  SetPendingCredentials(kUsername, kPassword);
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), two_forms_best_matches());
  MockPasswordEditDialog* mock_dialog = PreparePasswordEditDialog();
  // Verify parameters to Show() call.
  EXPECT_CALL(*mock_dialog, Show(ElementsAre(std::u16string(kUsername),
                                             std::u16string(kUsername2)),
                                 0, std::u16string(kPassword),
                                 std::u16string(kOrigin), std::string()));
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/true);
  TriggerActionClick();
  TriggerDialogDismissedCallback(/*dialog_accepted=*/false);
}

// Tests triggering password edit dialog and saving credentials after the
// user accepts the dialog.
TEST_F(SaveUpdatePasswordMessageDelegateTest, TriggerEditDialog_Accept) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), two_forms_best_matches());
  EXPECT_CALL(*form_manager, Save());
  MockPasswordEditDialog* mock_dialog = PreparePasswordEditDialog();
  EXPECT_CALL(*mock_dialog, Show);
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/true);
  EXPECT_NE(nullptr, GetMessageWrapper());
  TriggerActionClick();
  EXPECT_EQ(nullptr, GetMessageWrapper());
  TriggerDialogAcceptedCallback(/*selected_username_index=*/0);
  TriggerDialogDismissedCallback(/*dialog_accepted=*/true);

  CommitPasswordFormMetrics();
  VerifyUkmMetrics(
      test_ukm_recorder,
      PasswordFormMetricsRecorder::BubbleDismissalReason::kAccepted);
  histogram_tester.ExpectUniqueSample(
      kUpdateUIDismissalReasonHistogramName,
      password_manager::metrics_util::CLICKED_ACCEPT, 1);
}

// Tests that credentials are not saved if the user cancels password edit
// dialog.
TEST_F(SaveUpdatePasswordMessageDelegateTest, TriggerEditDialog_Cancel) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), two_forms_best_matches());
  EXPECT_CALL(*form_manager, Save).Times(0);
  MockPasswordEditDialog* mock_dialog = PreparePasswordEditDialog();
  EXPECT_CALL(*mock_dialog, Show);
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/true);
  EXPECT_NE(nullptr, GetMessageWrapper());
  TriggerActionClick();
  EXPECT_EQ(nullptr, GetMessageWrapper());
  TriggerDialogDismissedCallback(/*dialog_accepted=*/false);

  CommitPasswordFormMetrics();
  VerifyUkmMetrics(
      test_ukm_recorder,
      PasswordFormMetricsRecorder::BubbleDismissalReason::kDeclined);
  histogram_tester.ExpectUniqueSample(
      kUpdateUIDismissalReasonHistogramName,
      password_manager::metrics_util::CLICKED_CANCEL, 1);
}
