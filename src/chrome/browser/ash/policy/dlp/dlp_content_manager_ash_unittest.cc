// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dlp_content_manager_ash.h"

#include <memory>

#include "ash/public/cpp/privacy_screen_dlp_helper.h"
#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/memory/ptr_util.h"
#include "base/task/thread_pool.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/policy/dlp/dlp_content_manager_ash_test_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_histogram_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_policy_event.pb.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_reporting_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_reporting_manager_test_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_warn_dialog.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_warn_notifier.h"
#include "chrome/browser/chromeos/policy/dlp/mock_dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/mock_dlp_warn_notifier.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/screenshot_area.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/reporting/client/mock_report_queue.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using ::testing::_;
using ::testing::Mock;

namespace policy {

namespace {

constexpr char kEmailId[] = "test@example.com";
constexpr char kGaiaId[] = "12345";
constexpr char kSrcPattern[] = "example";

const DlpContentRestrictionSet kScreenshotRestricted(
    DlpContentRestriction::kScreenshot,
    DlpRulesManager::Level::kBlock);
const DlpContentRestrictionSet kPrivacyScreenEnforced(
    DlpContentRestriction::kPrivacyScreen,
    DlpRulesManager::Level::kBlock);
const DlpContentRestrictionSet kPrivacyScreenReported(
    DlpContentRestriction::kPrivacyScreen,
    DlpRulesManager::Level::kReport);
const DlpContentRestrictionSet kPrintingRestricted(
    DlpContentRestriction::kPrint,
    DlpRulesManager::Level::kBlock);

const DlpContentRestrictionSet kPrintingWarned(DlpContentRestriction::kPrint,
                                               DlpRulesManager::Level::kWarn);
const DlpContentRestrictionSet kScreenshotWarned(
    DlpContentRestriction::kScreenshot,
    DlpRulesManager::Level::kWarn);
const DlpContentRestrictionSet kScreenShareWarned(
    DlpContentRestriction::kScreenShare,
    DlpRulesManager::Level::kWarn);

const DlpContentRestrictionSet kEmptyRestrictionSet;
const DlpContentRestrictionSet kNonEmptyRestrictionSet = kScreenshotRestricted;

class MockPrivacyScreenHelper : public ash::PrivacyScreenDlpHelper {
 public:
  MOCK_METHOD(bool, IsSupported, (), (const, override));
  MOCK_METHOD(void, SetEnforced, (bool enforced), (override));
};

auto on_dlp_restriction_checked_callback = [](absl::optional<bool>* out_result,
                                              bool should_proceed) {
  *out_result = absl::make_optional(should_proceed);
};

}  // namespace

// TODO(crbug.com/1262948): Enable and modify for lacros.
class DlpContentManagerAshTest : public testing::Test {
 public:
  DlpContentManagerAshTest(const DlpContentManagerAshTest&) = delete;
  DlpContentManagerAshTest& operator=(const DlpContentManagerAshTest&) = delete;

  std::unique_ptr<KeyedService> SetDlpRulesManager(
      content::BrowserContext* context) {
    auto dlp_rules_manager = std::make_unique<MockDlpRulesManager>();
    mock_rules_manager_ = dlp_rules_manager.get();
    return dlp_rules_manager;
  }

 protected:
  DlpContentManagerAshTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()),
        user_manager_(new ash::FakeChromeUserManager()),
        scoped_user_manager_(base::WrapUnique(user_manager_)) {}
  ~DlpContentManagerAshTest() override = default;

  std::unique_ptr<content::WebContents> CreateWebContents() {
    return content::WebContentsTester::CreateTestWebContents(profile_, nullptr);
  }

  void SetUp() override {
    testing::Test::SetUp();

    ASSERT_TRUE(profile_manager_.SetUp());
    LoginFakeUser();

    EXPECT_CALL(mock_privacy_screen_helper_, IsSupported())
        .WillRepeatedly(::testing::Return(true));
  }

  void SetReportQueueForReportingManager() {
    auto report_queue = std::unique_ptr<::reporting::MockReportQueue,
                                        base::OnTaskRunnerDeleter>(
        new ::reporting::MockReportQueue(),
        base::OnTaskRunnerDeleter(
            base::ThreadPool::CreateSequencedTaskRunner({})));
    EXPECT_CALL(*report_queue.get(), AddRecord)
        .WillRepeatedly(
            [this](base::StringPiece record, reporting::Priority priority,
                   reporting::ReportQueue::EnqueueCallback callback) {
              DlpPolicyEvent event;
              event.ParseFromString(std::string(record));
              // Don't use this code in a multithreaded env as it can course
              // concurrency issues with the events in the vector.
              events_.push_back(event);
            });
    helper_.GetReportingManager()->SetReportQueueForTest(
        std::move(report_queue));
  }

  void SetupDlpRulesManager() {
    DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
        profile(),
        base::BindRepeating(&DlpContentManagerAshTest::SetDlpRulesManager,
                            base::Unretained(this)));
    ASSERT_TRUE(DlpRulesManagerFactory::GetForPrimaryProfile());
  }

  DlpContentManagerAsh* GetManager() { return helper_.GetContentManager(); }

  TestingProfile* profile() { return profile_; }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  DlpContentManagerAshTestHelper helper_;
  base::HistogramTester histogram_tester_;
  std::vector<DlpPolicyEvent> events_;
  MockDlpRulesManager* mock_rules_manager_ = nullptr;
  MockPrivacyScreenHelper mock_privacy_screen_helper_;

 private:
  void LoginFakeUser() {
    AccountId account_id = AccountId::FromUserEmailGaiaId(kEmailId, kGaiaId);

    profile_ = profile_manager_.CreateTestingProfile(account_id.GetUserEmail());
    profile_->SetIsNewProfile(true);

    user_manager_->AddUserWithAffiliationAndTypeAndProfile(
        account_id, false /*is_affiliated*/, user_manager::USER_TYPE_REGULAR,
        profile_);
    user_manager_->LoginUser(account_id, true /*set_profile_created_flag*/);

    EXPECT_EQ(ProfileManager::GetActiveUserProfile(), profile_);
  }

  content::RenderViewHostTestEnabler rvh_test_enabler_;
  TestingProfileManager profile_manager_;
  TestingProfile* profile_;
  ash::FakeChromeUserManager* user_manager_;
  user_manager::ScopedUserManager scoped_user_manager_;
};

TEST_F(DlpContentManagerAshTest, NoConfidentialDataShown) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetOnScreenPresentRestrictions(),
            kEmptyRestrictionSet);
}

TEST_F(DlpContentManagerAshTest, ConfidentialDataShown) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetOnScreenPresentRestrictions(),
            kEmptyRestrictionSet);

  helper_.ChangeConfidentiality(web_contents.get(), kNonEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kNonEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetOnScreenPresentRestrictions(),
            kNonEmptyRestrictionSet);

  helper_.DestroyWebContents(web_contents.get());
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetOnScreenPresentRestrictions(),
            kEmptyRestrictionSet);
}

TEST_F(DlpContentManagerAshTest, ConfidentialDataVisibilityChanged) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetOnScreenPresentRestrictions(),
            kEmptyRestrictionSet);

  helper_.ChangeConfidentiality(web_contents.get(), kNonEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kNonEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetOnScreenPresentRestrictions(),
            kNonEmptyRestrictionSet);

  web_contents->WasHidden();
  helper_.ChangeVisibility(web_contents.get());
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kNonEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetOnScreenPresentRestrictions(),
            kEmptyRestrictionSet);

  web_contents->WasShown();
  helper_.ChangeVisibility(web_contents.get());
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kNonEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetOnScreenPresentRestrictions(),
            kNonEmptyRestrictionSet);

  helper_.DestroyWebContents(web_contents.get());
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetOnScreenPresentRestrictions(),
            kEmptyRestrictionSet);
}

TEST_F(DlpContentManagerAshTest,
       TwoWebContentsVisibilityAndConfidentialityChanged) {
  std::unique_ptr<content::WebContents> web_contents1 = CreateWebContents();
  std::unique_ptr<content::WebContents> web_contents2 = CreateWebContents();
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents1.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents2.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetOnScreenPresentRestrictions(),
            kEmptyRestrictionSet);

  // WebContents 1 becomes confidential.
  helper_.ChangeConfidentiality(web_contents1.get(), kNonEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents1.get()),
            kNonEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents2.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetOnScreenPresentRestrictions(),
            kNonEmptyRestrictionSet);

  web_contents2->WasHidden();
  helper_.ChangeVisibility(web_contents2.get());
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents1.get()),
            kNonEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents2.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetOnScreenPresentRestrictions(),
            kNonEmptyRestrictionSet);

  // WebContents 1 becomes non-confidential.
  helper_.ChangeConfidentiality(web_contents1.get(), kEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents1.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents2.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetOnScreenPresentRestrictions(),
            kEmptyRestrictionSet);

  // WebContents 2 becomes confidential.
  helper_.ChangeConfidentiality(web_contents2.get(), kNonEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents1.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents2.get()),
            kNonEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetOnScreenPresentRestrictions(),
            kEmptyRestrictionSet);

  web_contents2->WasShown();
  helper_.ChangeVisibility(web_contents2.get());
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents1.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents2.get()),
            kNonEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetOnScreenPresentRestrictions(),
            kNonEmptyRestrictionSet);

  helper_.DestroyWebContents(web_contents1.get());
  helper_.DestroyWebContents(web_contents2.get());
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents1.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents2.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetOnScreenPresentRestrictions(),
            kEmptyRestrictionSet);
}

TEST_F(DlpContentManagerAshTest, PrivacyScreenEnforcement) {
  SetReportQueueForReportingManager();
  SetupDlpRulesManager();
  const std::string src_pattern("example.com");
  EXPECT_CALL(*mock_rules_manager_, GetSourceUrlPattern(_, _, _))
      .WillRepeatedly(::testing::Return(src_pattern));
  EXPECT_CALL(mock_privacy_screen_helper_, SetEnforced(testing::_)).Times(0);
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();

  testing::Mock::VerifyAndClearExpectations(&mock_privacy_screen_helper_);
  EXPECT_CALL(mock_privacy_screen_helper_, IsSupported())
      .WillRepeatedly(::testing::Return(true));
  EXPECT_CALL(mock_privacy_screen_helper_, SetEnforced(true)).Times(1);
  helper_.ChangeConfidentiality(web_contents.get(), kPrivacyScreenEnforced);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kPrivacyScreenEnforcedUMA, true, 1);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kPrivacyScreenEnforcedUMA, false, 0);
  EXPECT_EQ(events_.size(), 1u);
  EXPECT_THAT(events_[0],
              IsDlpPolicyEvent(CreateDlpPolicyEvent(
                  src_pattern, DlpRulesManager::Restriction::kPrivacyScreen,
                  DlpRulesManager::Level::kBlock)));

  testing::Mock::VerifyAndClearExpectations(&mock_privacy_screen_helper_);
  EXPECT_CALL(mock_privacy_screen_helper_, IsSupported())
      .WillRepeatedly(::testing::Return(true));
  EXPECT_CALL(mock_privacy_screen_helper_, SetEnforced(false)).Times(1);
  web_contents->WasHidden();
  helper_.ChangeVisibility(web_contents.get());
  task_environment_.FastForwardBy(helper_.GetPrivacyScreenOffDelay());
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kPrivacyScreenEnforcedUMA, true, 1);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kPrivacyScreenEnforcedUMA, false, 1);
  EXPECT_EQ(events_.size(), 1u);

  testing::Mock::VerifyAndClearExpectations(&mock_privacy_screen_helper_);
  EXPECT_CALL(mock_privacy_screen_helper_, IsSupported())
      .WillRepeatedly(::testing::Return(true));
  EXPECT_CALL(mock_privacy_screen_helper_, SetEnforced(true)).Times(1);
  web_contents->WasShown();
  helper_.ChangeVisibility(web_contents.get());
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kPrivacyScreenEnforcedUMA, true, 2);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kPrivacyScreenEnforcedUMA, false, 1);
  EXPECT_EQ(events_.size(), 2u);
  EXPECT_THAT(events_[1],
              IsDlpPolicyEvent(CreateDlpPolicyEvent(
                  src_pattern, DlpRulesManager::Restriction::kPrivacyScreen,
                  DlpRulesManager::Level::kBlock)));

  testing::Mock::VerifyAndClearExpectations(&mock_privacy_screen_helper_);
  EXPECT_CALL(mock_privacy_screen_helper_, IsSupported())
      .WillRepeatedly(::testing::Return(true));
  EXPECT_CALL(mock_privacy_screen_helper_, SetEnforced(false)).Times(1);
  helper_.DestroyWebContents(web_contents.get());
  task_environment_.FastForwardBy(helper_.GetPrivacyScreenOffDelay());
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kPrivacyScreenEnforcedUMA, true, 2);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kPrivacyScreenEnforcedUMA, false, 2);
  EXPECT_EQ(events_.size(), 2u);
}

TEST_F(DlpContentManagerAshTest, PrivacyScreenReported) {
  SetReportQueueForReportingManager();
  SetupDlpRulesManager();
  const std::string src_pattern("example.com");
  EXPECT_CALL(*mock_rules_manager_, GetSourceUrlPattern(_, _, _))
      .WillRepeatedly(::testing::Return(src_pattern));

  // Privacy screen should never be enforced.
  EXPECT_CALL(mock_privacy_screen_helper_, IsSupported())
      .WillRepeatedly(::testing::Return(true));
  EXPECT_CALL(mock_privacy_screen_helper_, SetEnforced(testing::_)).Times(0);
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();

  helper_.ChangeConfidentiality(web_contents.get(), kPrivacyScreenReported);
  EXPECT_EQ(events_.size(), 1u);
  EXPECT_THAT(events_[0],
              IsDlpPolicyEvent(CreateDlpPolicyEvent(
                  src_pattern, DlpRulesManager::Restriction::kPrivacyScreen,
                  DlpRulesManager::Level::kReport)));

  web_contents->WasHidden();
  helper_.ChangeVisibility(web_contents.get());
  task_environment_.FastForwardBy(helper_.GetPrivacyScreenOffDelay());
  EXPECT_EQ(events_.size(), 1u);

  web_contents->WasShown();
  helper_.ChangeVisibility(web_contents.get());
  EXPECT_EQ(events_.size(), 2u);
  EXPECT_THAT(events_[1],
              IsDlpPolicyEvent(CreateDlpPolicyEvent(
                  src_pattern, DlpRulesManager::Restriction::kPrivacyScreen,
                  DlpRulesManager::Level::kReport)));

  helper_.DestroyWebContents(web_contents.get());
  task_environment_.FastForwardBy(helper_.GetPrivacyScreenOffDelay());
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kPrivacyScreenEnforcedUMA, true, 0);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kPrivacyScreenEnforcedUMA, false, 0);
  EXPECT_EQ(events_.size(), 2u);
}

TEST_F(DlpContentManagerAshTest,
       PrivacyScreenNotEnforcedAndReportedOnUnsupportedDevice) {
  SetReportQueueForReportingManager();
  SetupDlpRulesManager();
  const std::string src_pattern("example.com");
  EXPECT_CALL(*mock_rules_manager_, GetSourceUrlPattern(_, _, _))
      .WillRepeatedly(::testing::Return(src_pattern));

  EXPECT_CALL(mock_privacy_screen_helper_, IsSupported())
      .WillRepeatedly(::testing::Return(false));

  // Privacy screen should never be enforced or reported.
  EXPECT_CALL(mock_privacy_screen_helper_, SetEnforced(testing::_)).Times(0);
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  helper_.ChangeConfidentiality(web_contents.get(), kPrivacyScreenEnforced);
  EXPECT_EQ(events_.size(), 0u);

  web_contents->WasHidden();
  helper_.ChangeVisibility(web_contents.get());
  task_environment_.FastForwardBy(helper_.GetPrivacyScreenOffDelay());
  EXPECT_EQ(events_.size(), 0u);

  web_contents->WasShown();
  helper_.ChangeVisibility(web_contents.get());
  EXPECT_EQ(events_.size(), 0u);

  helper_.DestroyWebContents(web_contents.get());
}

class DlpContentManagerAshCheckRestrictionTest
    : public DlpContentManagerAshTest {
 public:
  DlpContentManagerAshCheckRestrictionTest(
      const DlpContentManagerAshCheckRestrictionTest&) = delete;
  DlpContentManagerAshCheckRestrictionTest& operator=(
      const DlpContentManagerAshCheckRestrictionTest&) = delete;

 protected:
  DlpContentManagerAshCheckRestrictionTest() = default;

  void SetUp() override {
    DlpContentManagerAshTest::SetUp();

    SetReportQueueForReportingManager();
    SetupDlpRulesManager();
  }

  void TearDown() override {
    testing::Test::TearDown();

    helper_.ResetWarnNotifierForTesting();
    is_action_allowed_.reset();
  }

  MockDlpWarnNotifier* CreateAndSetDlpWarnNotifier(bool should_proceed) {
    std::unique_ptr<MockDlpWarnNotifier> wrapper =
        std::make_unique<MockDlpWarnNotifier>(should_proceed);
    MockDlpWarnNotifier* mock_dlp_warn_notifier = wrapper.get();
    helper_.SetWarnNotifierForTesting(std::move(wrapper));
    return mock_dlp_warn_notifier;
  }

  // Helper method that first verifies that is_action_allowed_ is correctly set
  // to hold a value that corresponds to |expected|, and then resets it to an
  // empty optional.
  void VerifyAndResetActionAllowed(bool expected) {
    ASSERT_TRUE(is_action_allowed_.has_value());
    EXPECT_EQ(is_action_allowed_.value(), expected);
  }

  absl::optional<bool> is_action_allowed_;
};

TEST_F(DlpContentManagerAshCheckRestrictionTest, PrintingRestricted) {
  // Needs to be set because CheckPrintingRestriction() will show the blocked
  // notification.
  NotificationDisplayServiceTester display_service_tester(profile());

  EXPECT_CALL(*mock_rules_manager_, GetSourceUrlPattern(_, _, _))
      .Times(1)
      .WillOnce(::testing::Return(kSrcPattern));

  // No restrictions are enforced for web_contents: allow.
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  GetManager()->CheckPrintingRestriction(
      web_contents.get(),
      base::BindOnce(on_dlp_restriction_checked_callback, &is_action_allowed_));
  VerifyAndResetActionAllowed(true /*expected*/);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kPrintingBlockedUMA, true, 0);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kPrintingBlockedUMA, false, 1);

  // Block restriction is enforced for web_contents: block.
  helper_.ChangeConfidentiality(web_contents.get(), kPrintingRestricted);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kPrintingRestricted);
  GetManager()->CheckPrintingRestriction(
      web_contents.get(),
      base::BindOnce(on_dlp_restriction_checked_callback, &is_action_allowed_));
  VerifyAndResetActionAllowed(false /*expected*/);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kPrintingBlockedUMA, true, 1);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kPrintingBlockedUMA, false, 1);

  EXPECT_EQ(events_.size(), 1u);
  EXPECT_THAT(events_[0],
              IsDlpPolicyEvent(CreateDlpPolicyEvent(
                  kSrcPattern, DlpRulesManager::Restriction::kPrinting,
                  DlpRulesManager::Level::kBlock)));

  // Web contents are destroyed: allow.
  helper_.DestroyWebContents(web_contents.get());
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  GetManager()->CheckPrintingRestriction(
      web_contents.get(),
      base::BindOnce(on_dlp_restriction_checked_callback, &is_action_allowed_));
  VerifyAndResetActionAllowed(true /*expected*/);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kPrintingBlockedUMA, true, 1);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kPrintingBlockedUMA, false, 2);
}

TEST_F(DlpContentManagerAshCheckRestrictionTest, PrintingWarnedContinued) {
  // Set the notifier to "Proceed" on the warning.
  MockDlpWarnNotifier* mock_dlp_warn_notifier =
      CreateAndSetDlpWarnNotifier(true /*should_proceed*/);
  // The warning should be shown only once.
  EXPECT_CALL(*mock_dlp_warn_notifier, ShowDlpWarningDialog(_, _)).Times(1);

  SetReportQueueForReportingManager();
  SetupDlpRulesManager();
  EXPECT_CALL(*mock_rules_manager_, GetSourceUrlPattern(_, _, _))
      .Times(3)
      .WillRepeatedly(::testing::Return(kSrcPattern));

  // No restrictions are enforced: allow.
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  EXPECT_FALSE(helper_.HasAnyContentCached());
  GetManager()->CheckPrintingRestriction(
      web_contents.get(),
      base::BindOnce(on_dlp_restriction_checked_callback, &is_action_allowed_));
  VerifyAndResetActionAllowed(true /*expected*/);
  EXPECT_FALSE(helper_.HasAnyContentCached());
  EXPECT_TRUE(events_.empty());

  // Warn restriction is enforced: allow and remember that the user proceeded.
  helper_.ChangeConfidentiality(web_contents.get(), kPrintingWarned);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kPrintingWarned);
  GetManager()->CheckPrintingRestriction(
      web_contents.get(),
      base::BindOnce(on_dlp_restriction_checked_callback, &is_action_allowed_));
  VerifyAndResetActionAllowed(true /*expected*/);
  EXPECT_TRUE(helper_.HasContentCachedForRestriction(
      web_contents.get(), DlpRulesManager::Restriction::kPrinting));
  EXPECT_EQ(events_.size(), 2u);
  EXPECT_THAT(events_[0],
              IsDlpPolicyEvent(CreateDlpPolicyEvent(
                  kSrcPattern, DlpRulesManager::Restriction::kPrinting,
                  DlpRulesManager::Level::kWarn)));
  EXPECT_THAT(events_[1],
              IsDlpPolicyEvent(CreateDlpPolicyWarningProceededEvent(
                  kSrcPattern, DlpRulesManager::Restriction::kPrinting)));

  // Check again: allow based on cached user's response - no dialog is shown.
  GetManager()->CheckPrintingRestriction(
      web_contents.get(),
      base::BindOnce(on_dlp_restriction_checked_callback, &is_action_allowed_));
  VerifyAndResetActionAllowed(true /*expected*/);
  EXPECT_TRUE(helper_.HasContentCachedForRestriction(
      web_contents.get(), DlpRulesManager::Restriction::kPrinting));
  EXPECT_EQ(events_.size(), 3u);
  EXPECT_THAT(events_[2],
              IsDlpPolicyEvent(CreateDlpPolicyWarningProceededEvent(
                  kSrcPattern, DlpRulesManager::Restriction::kPrinting)));

  // Web contents are destroyed: allow, no dialog is shown.
  helper_.DestroyWebContents(web_contents.get());
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  GetManager()->CheckPrintingRestriction(
      web_contents.get(),
      base::BindOnce(on_dlp_restriction_checked_callback, &is_action_allowed_));
  VerifyAndResetActionAllowed(true /*expected*/);
  EXPECT_EQ(events_.size(), 3u);
}

TEST_F(DlpContentManagerAshCheckRestrictionTest, PrintingWarnedCancelled) {
  // Set the notifier to "Proceed" on the warning.
  MockDlpWarnNotifier* mock_dlp_warn_notifier =
      CreateAndSetDlpWarnNotifier(false /*should_proceed*/);
  // If the user cancels, the warning can be shown again for the same contents.
  EXPECT_CALL(*mock_dlp_warn_notifier, ShowDlpWarningDialog(_, _)).Times(2);

  SetReportQueueForReportingManager();
  SetupDlpRulesManager();
  EXPECT_CALL(*mock_rules_manager_, GetSourceUrlPattern(_, _, _))
      .Times(2)
      .WillRepeatedly(::testing::Return(kSrcPattern));

  // No restrictions are enforced: allow.
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  EXPECT_FALSE(helper_.HasAnyContentCached());
  GetManager()->CheckPrintingRestriction(
      web_contents.get(),
      base::BindOnce(on_dlp_restriction_checked_callback, &is_action_allowed_));
  VerifyAndResetActionAllowed(true /*expected*/);
  EXPECT_FALSE(helper_.HasAnyContentCached());
  EXPECT_TRUE(events_.empty());

  // Warn restriction is enforced: reject since the user canceled.
  helper_.ChangeConfidentiality(web_contents.get(), kPrintingWarned);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kPrintingWarned);
  GetManager()->CheckPrintingRestriction(
      web_contents.get(),
      base::BindOnce(on_dlp_restriction_checked_callback, &is_action_allowed_));
  VerifyAndResetActionAllowed(false /*expected*/);
  EXPECT_FALSE(helper_.HasAnyContentCached());
  EXPECT_EQ(events_.size(), 1u);
  EXPECT_THAT(events_[0],
              IsDlpPolicyEvent(CreateDlpPolicyEvent(
                  kSrcPattern, DlpRulesManager::Restriction::kPrinting,
                  DlpRulesManager::Level::kWarn)));

  // Check again: since the user previously cancelled, dialog is shown again.
  GetManager()->CheckPrintingRestriction(
      web_contents.get(),
      base::BindOnce(on_dlp_restriction_checked_callback, &is_action_allowed_));
  VerifyAndResetActionAllowed(false /*expected*/);
  EXPECT_FALSE(helper_.HasAnyContentCached());
  EXPECT_EQ(events_.size(), 2u);
  EXPECT_THAT(events_[1],
              IsDlpPolicyEvent(CreateDlpPolicyEvent(
                  kSrcPattern, DlpRulesManager::Restriction::kPrinting,
                  DlpRulesManager::Level::kWarn)));

  // Web contents are destroyed: allow, no dialog is shown.
  helper_.DestroyWebContents(web_contents.get());
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  GetManager()->CheckPrintingRestriction(
      web_contents.get(),
      base::BindOnce(on_dlp_restriction_checked_callback, &is_action_allowed_));
  VerifyAndResetActionAllowed(true /*expected*/);
  EXPECT_EQ(events_.size(), 2u);
}

TEST_F(DlpContentManagerAshCheckRestrictionTest, CaptureModeInitRestricted) {
  // Needs to be set because CheckCaptureModeInitRestriction() will show the
  // blocked notification.
  NotificationDisplayServiceTester display_service_tester(profile());

  EXPECT_CALL(*mock_rules_manager_, GetSourceUrlPattern(_, _, _))
      .Times(1)
      .WillOnce(::testing::Return(kSrcPattern));

  // No restrictions are enforced: allow.
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  GetManager()->CheckCaptureModeInitRestriction(
      base::BindOnce(on_dlp_restriction_checked_callback, &is_action_allowed_));
  VerifyAndResetActionAllowed(true /*expected*/);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kCaptureModeInitBlockedUMA, true, 0);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kCaptureModeInitBlockedUMA, false, 1);

  // Block restriction is enforced for web_contents: block.
  helper_.ChangeConfidentiality(web_contents.get(), kScreenshotRestricted);

  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kScreenshotRestricted);
  GetManager()->CheckCaptureModeInitRestriction(
      base::BindOnce(on_dlp_restriction_checked_callback, &is_action_allowed_));
  VerifyAndResetActionAllowed(false /*expected*/);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kCaptureModeInitBlockedUMA, true, 1);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kCaptureModeInitBlockedUMA, false, 1);

  EXPECT_EQ(events_.size(), 1u);
  EXPECT_THAT(events_[0],
              IsDlpPolicyEvent(CreateDlpPolicyEvent(
                  kSrcPattern, DlpRulesManager::Restriction::kScreenshot,
                  DlpRulesManager::Level::kBlock)));

  // Web contents are destroyed: allow.
  helper_.DestroyWebContents(web_contents.get());
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  GetManager()->CheckCaptureModeInitRestriction(
      base::BindOnce(on_dlp_restriction_checked_callback, &is_action_allowed_));
  VerifyAndResetActionAllowed(true /*expected*/);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kCaptureModeInitBlockedUMA, true, 1);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kCaptureModeInitBlockedUMA, false, 2);
}

TEST_F(DlpContentManagerAshCheckRestrictionTest,
       CaptureModeInitWarnedContinued) {
  // Set the notifier to "Proceed" on the warning.
  MockDlpWarnNotifier* mock_dlp_warn_notifier =
      CreateAndSetDlpWarnNotifier(true /*should_proceed*/);
  // The warning should be shown only once.
  EXPECT_CALL(*mock_dlp_warn_notifier, ShowDlpWarningDialog(_, _)).Times(1);

  SetReportQueueForReportingManager();
  SetupDlpRulesManager();
  EXPECT_CALL(*mock_rules_manager_, GetSourceUrlPattern)
      .Times(3)
      .WillRepeatedly(::testing::Return(kSrcPattern));

  // No restrictions are enforced: allow.
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  EXPECT_FALSE(helper_.HasAnyContentCached());
  GetManager()->CheckCaptureModeInitRestriction(
      base::BindOnce(on_dlp_restriction_checked_callback, &is_action_allowed_));
  VerifyAndResetActionAllowed(true /*expected*/);
  EXPECT_FALSE(helper_.HasAnyContentCached());
  EXPECT_TRUE(events_.empty());

  // Warn restriction is enforced: allow and remember that the user proceeded.
  helper_.ChangeConfidentiality(web_contents.get(), kScreenshotWarned);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kScreenshotWarned);
  GetManager()->CheckCaptureModeInitRestriction(
      base::BindOnce(on_dlp_restriction_checked_callback, &is_action_allowed_));
  VerifyAndResetActionAllowed(true /*expected*/);
  EXPECT_TRUE(helper_.HasContentCachedForRestriction(
      web_contents.get(), DlpRulesManager::Restriction::kScreenshot));
  EXPECT_EQ(events_.size(), 2u);
  EXPECT_THAT(events_[0],
              IsDlpPolicyEvent(CreateDlpPolicyEvent(
                  kSrcPattern, DlpRulesManager::Restriction::kScreenshot,
                  DlpRulesManager::Level::kWarn)));
  EXPECT_THAT(events_[1],
              IsDlpPolicyEvent(CreateDlpPolicyWarningProceededEvent(
                  kSrcPattern, DlpRulesManager::Restriction::kScreenshot)));

  // Check again: allow based on cached user's response - no dialog is shown.
  GetManager()->CheckCaptureModeInitRestriction(
      base::BindOnce(on_dlp_restriction_checked_callback, &is_action_allowed_));
  VerifyAndResetActionAllowed(true /*expected*/);
  EXPECT_TRUE(helper_.HasContentCachedForRestriction(
      web_contents.get(), DlpRulesManager::Restriction::kScreenshot));
  EXPECT_EQ(events_.size(), 3u);
  EXPECT_THAT(events_[2],
              IsDlpPolicyEvent(CreateDlpPolicyWarningProceededEvent(
                  kSrcPattern, DlpRulesManager::Restriction::kScreenshot)));

  // Web contents are destroyed: allow, no dialog is shown.
  helper_.DestroyWebContents(web_contents.get());
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  GetManager()->CheckCaptureModeInitRestriction(
      base::BindOnce(on_dlp_restriction_checked_callback, &is_action_allowed_));
  VerifyAndResetActionAllowed(true /*expected*/);
  EXPECT_EQ(events_.size(), 3u);
}

TEST_F(DlpContentManagerAshCheckRestrictionTest,
       CaptureModeInitWarnedCancelled) {
  // Set the notifier to "Proceed" on the warning.
  MockDlpWarnNotifier* mock_dlp_warn_notifier =
      CreateAndSetDlpWarnNotifier(false /*should_proceed*/);
  // If the user cancels, the warning can be shown again for the same contents.
  EXPECT_CALL(*mock_dlp_warn_notifier, ShowDlpWarningDialog(_, _)).Times(2);

  SetReportQueueForReportingManager();
  SetupDlpRulesManager();
  EXPECT_CALL(*mock_rules_manager_, GetSourceUrlPattern)
      .Times(2)
      .WillRepeatedly(::testing::Return(kSrcPattern));

  // No restrictions are enforced: allow.
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  EXPECT_FALSE(helper_.HasAnyContentCached());
  GetManager()->CheckCaptureModeInitRestriction(
      base::BindOnce(on_dlp_restriction_checked_callback, &is_action_allowed_));
  VerifyAndResetActionAllowed(true /*expected*/);
  EXPECT_FALSE(helper_.HasAnyContentCached());
  EXPECT_TRUE(events_.empty());

  // Warn restriction is enforced: reject since the user canceled.
  helper_.ChangeConfidentiality(web_contents.get(), kScreenshotWarned);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kScreenshotWarned);
  GetManager()->CheckCaptureModeInitRestriction(
      base::BindOnce(on_dlp_restriction_checked_callback, &is_action_allowed_));
  VerifyAndResetActionAllowed(false /*expected*/);
  EXPECT_FALSE(helper_.HasAnyContentCached());
  EXPECT_EQ(events_.size(), 1u);
  EXPECT_THAT(events_[0],
              IsDlpPolicyEvent(CreateDlpPolicyEvent(
                  kSrcPattern, DlpRulesManager::Restriction::kScreenshot,
                  DlpRulesManager::Level::kWarn)));

  // Check again: since the user previously cancelled, dialog is shown again.
  GetManager()->CheckCaptureModeInitRestriction(
      base::BindOnce(on_dlp_restriction_checked_callback, &is_action_allowed_));
  VerifyAndResetActionAllowed(false /*expected*/);
  EXPECT_FALSE(helper_.HasAnyContentCached());
  EXPECT_EQ(events_.size(), 2u);
  EXPECT_THAT(events_[1],
              IsDlpPolicyEvent(CreateDlpPolicyEvent(
                  kSrcPattern, DlpRulesManager::Restriction::kScreenshot,
                  DlpRulesManager::Level::kWarn)));

  // Web contents are destroyed: allow, no dialog is shown.
  helper_.DestroyWebContents(web_contents.get());
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  GetManager()->CheckCaptureModeInitRestriction(
      base::BindOnce(on_dlp_restriction_checked_callback, &is_action_allowed_));

  VerifyAndResetActionAllowed(true /*expected*/);
  EXPECT_EQ(events_.size(), 2u);
}

TEST_F(DlpContentManagerAshCheckRestrictionTest, ScreenshotRestricted) {
  // Needs to be set because CheckScreenshotRestriction() will show the blocked
  // notification.
  NotificationDisplayServiceTester display_service_tester(profile());

  EXPECT_CALL(*mock_rules_manager_, GetSourceUrlPattern(_, _, _))
      .Times(1)
      .WillOnce(::testing::Return(kSrcPattern));

  ScreenshotArea area = ScreenshotArea::CreateForAllRootWindows();

  // No restrictions are enforced: allow.
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  GetManager()->CheckScreenshotRestriction(
      area,
      base::BindOnce(on_dlp_restriction_checked_callback, &is_action_allowed_));
  VerifyAndResetActionAllowed(true /*expected*/);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenshotBlockedUMA, true, 0);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenshotBlockedUMA, false, 1);

  // Block restriction is enforced for web_contents: block.
  helper_.ChangeConfidentiality(web_contents.get(), kScreenshotRestricted);

  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kScreenshotRestricted);
  GetManager()->CheckScreenshotRestriction(
      area,
      base::BindOnce(on_dlp_restriction_checked_callback, &is_action_allowed_));
  VerifyAndResetActionAllowed(false /*expected*/);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenshotBlockedUMA, true, 1);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenshotBlockedUMA, false, 1);

  EXPECT_EQ(events_.size(), 1u);
  EXPECT_THAT(events_[0],
              IsDlpPolicyEvent(CreateDlpPolicyEvent(
                  kSrcPattern, DlpRulesManager::Restriction::kScreenshot,
                  DlpRulesManager::Level::kBlock)));

  // Web contents are destroyed: allow.
  helper_.DestroyWebContents(web_contents.get());
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  GetManager()->CheckScreenshotRestriction(
      area,
      base::BindOnce(on_dlp_restriction_checked_callback, &is_action_allowed_));
  VerifyAndResetActionAllowed(true /*expected*/);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenshotBlockedUMA, true, 1);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kScreenshotBlockedUMA, false, 2);
}

TEST_F(DlpContentManagerAshCheckRestrictionTest, ScreenshotWarnedContinued) {
  // Set the notifier to "Proceed" on the warning.
  MockDlpWarnNotifier* mock_dlp_warn_notifier =
      CreateAndSetDlpWarnNotifier(true /*should_proceed*/);
  // The warning should be shown only once.
  EXPECT_CALL(*mock_dlp_warn_notifier, ShowDlpWarningDialog(_, _)).Times(1);

  SetReportQueueForReportingManager();
  SetupDlpRulesManager();
  EXPECT_CALL(*mock_rules_manager_, GetSourceUrlPattern)
      .Times(3)
      .WillRepeatedly(::testing::Return(kSrcPattern));

  ScreenshotArea area = ScreenshotArea::CreateForAllRootWindows();

  // No restrictions are enforced: allow.
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  EXPECT_FALSE(helper_.HasAnyContentCached());
  GetManager()->CheckScreenshotRestriction(
      area,
      base::BindOnce(on_dlp_restriction_checked_callback, &is_action_allowed_));
  VerifyAndResetActionAllowed(true /*expected*/);
  EXPECT_FALSE(helper_.HasAnyContentCached());
  EXPECT_TRUE(events_.empty());

  // Warn restriction is enforced: allow and remember that the user proceeded.
  helper_.ChangeConfidentiality(web_contents.get(), kScreenshotWarned);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kScreenshotWarned);
  GetManager()->CheckScreenshotRestriction(
      area,
      base::BindOnce(on_dlp_restriction_checked_callback, &is_action_allowed_));
  VerifyAndResetActionAllowed(true /*expected*/);
  EXPECT_TRUE(helper_.HasContentCachedForRestriction(
      web_contents.get(), DlpRulesManager::Restriction::kScreenshot));
  EXPECT_EQ(events_.size(), 2u);
  EXPECT_THAT(events_[0],
              IsDlpPolicyEvent(CreateDlpPolicyEvent(
                  kSrcPattern, DlpRulesManager::Restriction::kScreenshot,
                  DlpRulesManager::Level::kWarn)));
  EXPECT_THAT(events_[1],
              IsDlpPolicyEvent(CreateDlpPolicyWarningProceededEvent(
                  kSrcPattern, DlpRulesManager::Restriction::kScreenshot)));

  // Check again: allow based on cached user's response - no dialog is shown.
  GetManager()->CheckScreenshotRestriction(
      area,
      base::BindOnce(on_dlp_restriction_checked_callback, &is_action_allowed_));
  VerifyAndResetActionAllowed(true /*expected*/);
  EXPECT_TRUE(helper_.HasContentCachedForRestriction(
      web_contents.get(), DlpRulesManager::Restriction::kScreenshot));
  EXPECT_EQ(events_.size(), 3u);
  EXPECT_THAT(events_[2],
              IsDlpPolicyEvent(CreateDlpPolicyWarningProceededEvent(
                  kSrcPattern, DlpRulesManager::Restriction::kScreenshot)));

  // Web contents are destroyed: allow, no dialog is shown.
  helper_.DestroyWebContents(web_contents.get());
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  GetManager()->CheckScreenshotRestriction(
      area,
      base::BindOnce(on_dlp_restriction_checked_callback, &is_action_allowed_));
  VerifyAndResetActionAllowed(true /*expected*/);
  EXPECT_EQ(events_.size(), 3u);
}

TEST_F(DlpContentManagerAshCheckRestrictionTest, ScreenshotWarnedCancelled) {
  // Set the notifier to "Proceed" on the warning.
  MockDlpWarnNotifier* mock_dlp_warn_notifier =
      CreateAndSetDlpWarnNotifier(false /*should_proceed*/);
  // If the user cancels, the warning can be shown again for the same contents.
  EXPECT_CALL(*mock_dlp_warn_notifier, ShowDlpWarningDialog(_, _)).Times(2);

  SetReportQueueForReportingManager();
  SetupDlpRulesManager();
  EXPECT_CALL(*mock_rules_manager_, GetSourceUrlPattern)
      .Times(2)
      .WillRepeatedly(::testing::Return(kSrcPattern));

  ScreenshotArea area = ScreenshotArea::CreateForAllRootWindows();

  // No restrictions are enforced: allow.
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  EXPECT_FALSE(helper_.HasAnyContentCached());
  GetManager()->CheckScreenshotRestriction(
      area,
      base::BindOnce(on_dlp_restriction_checked_callback, &is_action_allowed_));
  VerifyAndResetActionAllowed(true /*expected*/);
  EXPECT_FALSE(helper_.HasAnyContentCached());
  EXPECT_TRUE(events_.empty());

  // Warn restriction is enforced: reject since the user canceled.
  helper_.ChangeConfidentiality(web_contents.get(), kScreenshotWarned);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kScreenshotWarned);
  GetManager()->CheckScreenshotRestriction(
      area,
      base::BindOnce(on_dlp_restriction_checked_callback, &is_action_allowed_));
  VerifyAndResetActionAllowed(false /*expected*/);
  EXPECT_FALSE(helper_.HasAnyContentCached());
  EXPECT_EQ(events_.size(), 1u);
  EXPECT_THAT(events_[0],
              IsDlpPolicyEvent(CreateDlpPolicyEvent(
                  kSrcPattern, DlpRulesManager::Restriction::kScreenshot,
                  DlpRulesManager::Level::kWarn)));

  // Check again: since the user previously cancelled, dialog is shown again.
  GetManager()->CheckScreenshotRestriction(
      area,
      base::BindOnce(on_dlp_restriction_checked_callback, &is_action_allowed_));
  VerifyAndResetActionAllowed(false /*expected*/);
  EXPECT_FALSE(helper_.HasAnyContentCached());
  EXPECT_EQ(events_.size(), 2u);
  EXPECT_THAT(events_[1],
              IsDlpPolicyEvent(CreateDlpPolicyEvent(
                  kSrcPattern, DlpRulesManager::Restriction::kScreenshot,
                  DlpRulesManager::Level::kWarn)));

  // Web contents are destroyed: allow, no dialog is shown.
  helper_.DestroyWebContents(web_contents.get());
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  GetManager()->CheckScreenshotRestriction(
      area,
      base::BindOnce(on_dlp_restriction_checked_callback, &is_action_allowed_));
  VerifyAndResetActionAllowed(true /*expected*/);
  EXPECT_EQ(events_.size(), 2u);
}

}  // namespace policy
