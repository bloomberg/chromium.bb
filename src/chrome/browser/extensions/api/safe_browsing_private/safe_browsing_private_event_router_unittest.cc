// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"

#include <utility>

#include "base/bind.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/safe_browsing_private.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/proto/webprotect.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/test_event_router.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chromeos/tpm/stub_install_attributes.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#endif

using ::testing::_;
using ::testing::Mock;
using ::testing::SaveArg;

namespace extensions {

namespace {

ACTION_P(CaptureArg, wrapper) {
  *wrapper = arg0.Clone();
}

}  // namespace

class FakeAuthorizedSafeBrowsingPrivateEventRouter
    : public SafeBrowsingPrivateEventRouter {
 public:
  explicit FakeAuthorizedSafeBrowsingPrivateEventRouter(
      content::BrowserContext* context)
      : SafeBrowsingPrivateEventRouter(context) {}

 private:
  void ReportRealtimeEvent(const std::string& name,
                           EventBuilder event_builder) override {
    ReportRealtimeEventCallback(name, std::move(event_builder), true);
  }
};

class FakeUnauthorizedSafeBrowsingPrivateEventRouter
    : public SafeBrowsingPrivateEventRouter {
 public:
  explicit FakeUnauthorizedSafeBrowsingPrivateEventRouter(
      content::BrowserContext* context)
      : SafeBrowsingPrivateEventRouter(context) {}

 private:
  void ReportRealtimeEvent(const std::string& name,
                           EventBuilder event_builder) override {
    ReportRealtimeEventCallback(name, std::move(event_builder), false);
  }
};

class SafeBrowsingEventObserver : public TestEventRouter::EventObserver {
 public:
  // The observer will only listen to events with the |event_name|.
  explicit SafeBrowsingEventObserver(const std::string& event_name)
      : event_name_(event_name) {}

  ~SafeBrowsingEventObserver() override = default;

  // Removes |event_args_| from |*this| and returns them.
  base::Value PassEventArgs() { return std::move(event_args_); }

  // extensions::TestEventRouter::EventObserver:
  void OnBroadcastEvent(const extensions::Event& event) override {
    if (event.event_name == event_name_) {
      event_args_ = event.event_args->Clone();
    }
  }

 private:
  // The name of the observed event.
  const std::string event_name_;

  // The arguments passed for the last observed event.
  base::Value event_args_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingEventObserver);
};

std::unique_ptr<KeyedService> BuildSafeBrowsingPrivateEventRouter(
    bool authorized,
    content::BrowserContext* context) {
  if (authorized)
    return std::unique_ptr<KeyedService>(
        new FakeAuthorizedSafeBrowsingPrivateEventRouter(context));
  else
    return std::unique_ptr<KeyedService>(
        new FakeUnauthorizedSafeBrowsingPrivateEventRouter(context));
}

class SafeBrowsingPrivateEventRouterTest : public testing::Test {
 public:
  SafeBrowsingPrivateEventRouterTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");
  }

  ~SafeBrowsingPrivateEventRouterTest() override = default;

  void TriggerOnPolicySpecifiedPasswordReuseDetectedEvent() {
    SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile_)
        ->OnPolicySpecifiedPasswordReuseDetected(GURL("https://phishing.com/"),
                                                 "user_name_1",
                                                 /*is_phishing_url*/ true);
  }

  void TriggerOnPolicySpecifiedPasswordChangedEvent() {
    SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile_)
        ->OnPolicySpecifiedPasswordChanged("user_name_2");
  }

  void TriggerOnDangerousDownloadOpenedEvent() {
    SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile_)
        ->OnDangerousDownloadOpened(GURL("https://evil.com/malware.exe"),
                                    "/path/to/malware.exe",
                                    "sha256_of_malware_exe", "exe", 1234);
  }

  void TriggerOnSecurityInterstitialShownEvent() {
    SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile_)
        ->OnSecurityInterstitialShown(GURL("https://phishing.com/"), "PHISHING",
                                      0);
  }

  void TriggerOnSecurityInterstitialProceededEvent() {
    SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile_)
        ->OnSecurityInterstitialProceeded(GURL("https://phishing.com/"),
                                          "PHISHING", -201);
  }

  void TriggerOnDangerousDownloadWarningEvent() {
    SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile_)
        ->OnDangerousDownloadWarning(
            GURL("https://maybevil.com/warning.exe"), "/path/to/warning.exe",
            "sha256_of_warning_exe", "POTENTIALLY_UNWANTED", "exe", 567);
  }

  void TriggerOnDangerousDownloadWarningEventBypass() {
    SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile_)
        ->OnDangerousDownloadWarning(
            GURL("https://bypassevil.com/bypass.exe"), "/path/to/bypass.exe",
            "sha256_of_bypass_exe", "BYPASSED_WARNING", "exe", 890);
  }

  void TriggerOnSensitiveDataEvent() {
    safe_browsing::DlpDeepScanningVerdict verdict;
    verdict.set_status(safe_browsing::DlpDeepScanningVerdict::SUCCESS);
    safe_browsing::DlpDeepScanningVerdict::TriggeredRule* rule =
        verdict.add_triggered_rules();
    rule->set_action(
        safe_browsing::DlpDeepScanningVerdict::TriggeredRule::BLOCK);
    rule->set_rule_name("fake rule");
    rule->set_rule_id(12345);
    rule->set_rule_resource_name("fake resource name");
    rule->set_rule_severity("fake severity");

    safe_browsing::DlpDeepScanningVerdict::MatchedDetector* detector =
        rule->add_matched_detectors();
    detector->set_detector_id("fake id");
    detector->set_display_name("fake name");
    detector->set_detector_type("fake type");

    detector = rule->add_matched_detectors();
    detector->set_detector_id("fake id2");
    detector->set_display_name("fake name2");
    detector->set_detector_type("fake type2");

    SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile_)
        ->OnSensitiveDataEvent(verdict,
                               GURL("https://evil.com/sensitive_data.txt"),
                               "sensitive_data.txt", "sha256_of_data",
                               "text/plain", "FILE_UPLOAD", 12345);
  }

  void TriggerOnUnscannedFileEvent() {
    SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile_)
        ->OnUnscannedFileEvent(GURL("https://evil.com/sensitive_data.txt"),
                               "sensitive_data.txt", "sha256_of_data",
                               "text/plain", "FILE_DOWNLOAD",
                               "filePasswordProtected", 12345);
  }

  void SetReportingPolicy(bool enabled) {
    TestingBrowserProcess::GetGlobal()->local_state()->SetBoolean(
        prefs::kUnsafeEventsReportingEnabled, enabled);

    // If we are not enabling reporting, or if the client has already been
    // set for testing, just return.
    if (!enabled || client_)
      return;

    // Set a mock cloud policy client in the router.
    client_ = std::make_unique<policy::MockCloudPolicyClient>();
    SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile_)
        ->SetCloudPolicyClientForTesting(client_.get());
  }

  void SetUpRouters(bool realtime_reporting_enable = true,
                    bool authorized = true) {
    event_router_ = extensions::CreateAndUseTestEventRouter(profile_);
    SafeBrowsingPrivateEventRouterFactory::GetInstance()->SetTestingFactory(
        profile_,
        base::BindRepeating(&BuildSafeBrowsingPrivateEventRouter, authorized));

    SetReportingPolicy(realtime_reporting_enable);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<policy::MockCloudPolicyClient> client_;
  TestingProfileManager profile_manager_;
  TestingProfile* profile_;
  extensions::TestEventRouter* event_router_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingPrivateEventRouterTest);
};

TEST_F(SafeBrowsingPrivateEventRouterTest, TestOnReuseDetected) {
  SetUpRouters();
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnPolicySpecifiedPasswordReuseDetected::
          kEventName);
  event_router_->AddEventObserver(&event_observer);

  base::Value report;
  EXPECT_CALL(*client_, UploadRealtimeReport_(_, _))
      .WillOnce(CaptureArg(&report));

  TriggerOnPolicySpecifiedPasswordReuseDetectedEvent();
  base::RunLoop().RunUntilIdle();

  auto captured_args = event_observer.PassEventArgs().GetList()[0].Clone();
  EXPECT_EQ("https://phishing.com/", captured_args.FindKey("url")->GetString());
  EXPECT_EQ("user_name_1", captured_args.FindKey("userName")->GetString());

  Mock::VerifyAndClearExpectations(client_.get());
  EXPECT_EQ(base::Value::Type::DICTIONARY, report.type());
  base::Value* event_list =
      report.FindKey(policy::RealtimeReportingJobConfiguration::kEventListKey);
  ASSERT_NE(nullptr, event_list);
  EXPECT_EQ(base::Value::Type::LIST, event_list->type());
  base::Value::ListView mutable_list = event_list->GetList();
  ASSERT_EQ(1, (int)mutable_list.size());
  base::Value wrapper = std::move(mutable_list[0]);
  EXPECT_EQ(base::Value::Type::DICTIONARY, wrapper.type());
  base::Value* event =
      wrapper.FindKey(SafeBrowsingPrivateEventRouter::kKeyPasswordReuseEvent);
  EXPECT_NE(nullptr, event);
  EXPECT_EQ("https://phishing.com/",
            *event->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyUrl));
  EXPECT_EQ("user_name_1", *event->FindStringKey(
                               SafeBrowsingPrivateEventRouter::kKeyUserName));
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestOnPasswordChanged) {
  SetUpRouters();
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnPolicySpecifiedPasswordChanged::kEventName);
  event_router_->AddEventObserver(&event_observer);

  base::Value report;
  EXPECT_CALL(*client_, UploadRealtimeReport_(_, _))
      .WillOnce(CaptureArg(&report));

  TriggerOnPolicySpecifiedPasswordChangedEvent();
  base::RunLoop().RunUntilIdle();

  auto captured_args = event_observer.PassEventArgs().GetList()[0].Clone();
  EXPECT_EQ("user_name_2", captured_args.GetString());

  Mock::VerifyAndClearExpectations(client_.get());
  EXPECT_EQ(base::Value::Type::DICTIONARY, report.type());
  base::Value* event_list =
      report.FindKey(policy::RealtimeReportingJobConfiguration::kEventListKey);
  ASSERT_NE(nullptr, event_list);
  EXPECT_EQ(base::Value::Type::LIST, event_list->type());
  base::Value::ListView mutable_list = event_list->GetList();
  ASSERT_EQ(1, (int)mutable_list.size());
  base::Value wrapper = std::move(mutable_list[0]);
  EXPECT_EQ(base::Value::Type::DICTIONARY, wrapper.type());
  base::Value* event =
      wrapper.FindKey(SafeBrowsingPrivateEventRouter::kKeyPasswordChangedEvent);
  EXPECT_NE(nullptr, event);
  EXPECT_EQ("user_name_2", *event->FindStringKey(
                               SafeBrowsingPrivateEventRouter::kKeyUserName));
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestOnDangerousDownloadOpened) {
  SetUpRouters();
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnDangerousDownloadOpened::kEventName);
  event_router_->AddEventObserver(&event_observer);

  base::Value report;
  EXPECT_CALL(*client_, UploadRealtimeReport_(_, _))
      .WillOnce(CaptureArg(&report));

  TriggerOnDangerousDownloadOpenedEvent();
  base::RunLoop().RunUntilIdle();

  auto captured_args = event_observer.PassEventArgs().GetList()[0].Clone();
  EXPECT_EQ("https://evil.com/malware.exe",
            captured_args.FindKey("url")->GetString());
  EXPECT_EQ("/path/to/malware.exe",
            captured_args.FindKey("fileName")->GetString());
  EXPECT_EQ("", captured_args.FindKey("userName")->GetString());
  EXPECT_EQ("sha256_of_malware_exe",
            captured_args.FindKey("downloadDigestSha256")->GetString());

  Mock::VerifyAndClearExpectations(client_.get());
  EXPECT_EQ(base::Value::Type::DICTIONARY, report.type());
  base::Value* event_list =
      report.FindKey(policy::RealtimeReportingJobConfiguration::kEventListKey);
  ASSERT_NE(nullptr, event_list);
  EXPECT_EQ(base::Value::Type::LIST, event_list->type());
  base::Value::ListView mutable_list = event_list->GetList();
  ASSERT_EQ(1, (int)mutable_list.size());
  base::Value wrapper = std::move(mutable_list[0]);
  EXPECT_EQ(base::Value::Type::DICTIONARY, wrapper.type());
  base::Value* event = wrapper.FindKey(
      SafeBrowsingPrivateEventRouter::kKeyDangerousDownloadEvent);
  EXPECT_NE(nullptr, event);
  EXPECT_EQ(
      "/path/to/malware.exe",
      *event->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyFileName));
  EXPECT_EQ("exe", *event->FindStringKey(
                       SafeBrowsingPrivateEventRouter::kKeyContentType));
  EXPECT_EQ(1234, *event->FindIntKey(
                      SafeBrowsingPrivateEventRouter::kKeyContentSize));
  EXPECT_EQ(SafeBrowsingPrivateEventRouter::kTriggerFileDownload,
            *event->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyTrigger));
}

TEST_F(SafeBrowsingPrivateEventRouterTest,
       TestOnSecurityInterstitialProceeded) {
  SetUpRouters();
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnSecurityInterstitialProceeded::kEventName);
  event_router_->AddEventObserver(&event_observer);

  base::Value report;
  EXPECT_CALL(*client_, UploadRealtimeReport_(_, _))
      .WillOnce(CaptureArg(&report));

  TriggerOnSecurityInterstitialProceededEvent();
  base::RunLoop().RunUntilIdle();

  auto captured_args = event_observer.PassEventArgs().GetList()[0].Clone();
  EXPECT_EQ("https://phishing.com/", captured_args.FindKey("url")->GetString());
  EXPECT_EQ("PHISHING", captured_args.FindKey("reason")->GetString());
  EXPECT_EQ("-201", captured_args.FindKey("netErrorCode")->GetString());
  EXPECT_EQ("", captured_args.FindKey("userName")->GetString());

  Mock::VerifyAndClearExpectations(client_.get());
  EXPECT_EQ(base::Value::Type::DICTIONARY, report.type());
  base::Value* event_list =
      report.FindKey(policy::RealtimeReportingJobConfiguration::kEventListKey);
  ASSERT_NE(nullptr, event_list);
  EXPECT_EQ(base::Value::Type::LIST, event_list->type());
  base::Value::ListView mutable_list = event_list->GetList();
  ASSERT_EQ(1, (int)mutable_list.size());
  base::Value wrapper = std::move(mutable_list[0]);
  EXPECT_EQ(base::Value::Type::DICTIONARY, wrapper.type());
  base::Value* event =
      wrapper.FindKey(SafeBrowsingPrivateEventRouter::kKeyInterstitialEvent);
  EXPECT_NE(nullptr, event);
  EXPECT_EQ("PHISHING",
            *event->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyReason));
  EXPECT_EQ(-201, *event->FindIntKey(
                      SafeBrowsingPrivateEventRouter::kKeyNetErrorCode));
  EXPECT_TRUE(
      *event->FindBoolKey(SafeBrowsingPrivateEventRouter::kKeyClickedThrough));
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestOnSecurityInterstitialShown) {
  SetUpRouters();
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnSecurityInterstitialShown::kEventName);
  event_router_->AddEventObserver(&event_observer);

  base::Value report;
  EXPECT_CALL(*client_, UploadRealtimeReport_(_, _))
      .WillOnce(CaptureArg(&report));

  TriggerOnSecurityInterstitialShownEvent();
  base::RunLoop().RunUntilIdle();

  auto captured_args = event_observer.PassEventArgs().GetList()[0].Clone();
  EXPECT_EQ("https://phishing.com/", captured_args.FindKey("url")->GetString());
  EXPECT_EQ("PHISHING", captured_args.FindKey("reason")->GetString());
  EXPECT_FALSE(captured_args.FindKey("netErrorCode"));
  EXPECT_EQ("", captured_args.FindKey("userName")->GetString());

  Mock::VerifyAndClearExpectations(client_.get());
  EXPECT_EQ(base::Value::Type::DICTIONARY, report.type());
  base::Value* event_list =
      report.FindKey(policy::RealtimeReportingJobConfiguration::kEventListKey);
  ASSERT_NE(nullptr, event_list);
  EXPECT_EQ(base::Value::Type::LIST, event_list->type());
  base::Value::ListView mutable_list = event_list->GetList();
  ASSERT_EQ(1, (int)mutable_list.size());
  base::Value wrapper = std::move(mutable_list[0]);
  EXPECT_EQ(base::Value::Type::DICTIONARY, wrapper.type());
  base::Value* event =
      wrapper.FindKey(SafeBrowsingPrivateEventRouter::kKeyInterstitialEvent);
  EXPECT_NE(nullptr, event);
  EXPECT_EQ("PHISHING",
            *event->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyReason));
  EXPECT_EQ(
      0, *event->FindIntKey(SafeBrowsingPrivateEventRouter::kKeyNetErrorCode));
  EXPECT_FALSE(
      *event->FindBoolKey(SafeBrowsingPrivateEventRouter::kKeyClickedThrough));
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestOnDangerousDownloadWarning) {
  SetUpRouters();
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnDangerousDownloadOpened::kEventName);
  event_router_->AddEventObserver(&event_observer);

  base::Value report;
  EXPECT_CALL(*client_, UploadRealtimeReport_(_, _))
      .WillOnce(CaptureArg(&report));

  TriggerOnDangerousDownloadWarningEvent();
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(client_.get());
  EXPECT_EQ(base::Value::Type::DICTIONARY, report.type());
  base::Value* event_list =
      report.FindKey(policy::RealtimeReportingJobConfiguration::kEventListKey);
  ASSERT_NE(nullptr, event_list);
  EXPECT_EQ(base::Value::Type::LIST, event_list->type());
  base::Value::ListView mutable_list = event_list->GetList();
  ASSERT_EQ(1, (int)mutable_list.size());
  base::Value wrapper = std::move(mutable_list[0]);
  EXPECT_EQ(base::Value::Type::DICTIONARY, wrapper.type());
  base::Value* event = wrapper.FindKey(
      SafeBrowsingPrivateEventRouter::kKeyDangerousDownloadEvent);
  EXPECT_NE(nullptr, event);
  EXPECT_EQ(
      "/path/to/warning.exe",
      *event->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyFileName));
  EXPECT_EQ("exe", *event->FindStringKey(
                       SafeBrowsingPrivateEventRouter::kKeyContentType));
  EXPECT_EQ(
      567, *event->FindIntKey(SafeBrowsingPrivateEventRouter::kKeyContentSize));
  EXPECT_EQ(
      "POTENTIALLY_UNWANTED",
      *event->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyThreatType));
}

TEST_F(SafeBrowsingPrivateEventRouterTest,
       TestOnDangerousDownloadWarningBypass) {
  SetUpRouters();
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnDangerousDownloadOpened::kEventName);
  event_router_->AddEventObserver(&event_observer);

  base::Value report;
  EXPECT_CALL(*client_, UploadRealtimeReport_(_, _))
      .WillOnce(CaptureArg(&report));

  TriggerOnDangerousDownloadWarningEventBypass();
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(client_.get());
  EXPECT_EQ(base::Value::Type::DICTIONARY, report.type());
  base::Value* event_list =
      report.FindKey(policy::RealtimeReportingJobConfiguration::kEventListKey);
  ASSERT_NE(nullptr, event_list);
  EXPECT_EQ(base::Value::Type::LIST, event_list->type());
  base::Value::ListView mutable_list = event_list->GetList();
  ASSERT_EQ(1, (int)mutable_list.size());
  base::Value wrapper = std::move(mutable_list[0]);
  EXPECT_EQ(base::Value::Type::DICTIONARY, wrapper.type());
  base::Value* event = wrapper.FindKey(
      SafeBrowsingPrivateEventRouter::kKeyDangerousDownloadEvent);
  EXPECT_NE(nullptr, event);
  EXPECT_EQ(
      "/path/to/bypass.exe",
      *event->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyFileName));
  EXPECT_EQ("exe", *event->FindStringKey(
                       SafeBrowsingPrivateEventRouter::kKeyContentType));
  EXPECT_EQ(
      890, *event->FindIntKey(SafeBrowsingPrivateEventRouter::kKeyContentSize));
  EXPECT_EQ(
      "BYPASSED_WARNING",
      *event->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyThreatType));
}

TEST_F(SafeBrowsingPrivateEventRouterTest, PolicyControlOnToOffIsDynamic) {
  SetUpRouters();
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnSecurityInterstitialShown::kEventName);
  event_router_->AddEventObserver(&event_observer);

  EXPECT_CALL(*client_, UploadRealtimeReport_(_, _)).Times(1);
  TriggerOnSecurityInterstitialShownEvent();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, event_observer.PassEventArgs().GetList().size());
  Mock::VerifyAndClearExpectations(client_.get());

  // Now turn off policy.  This time no report should be generated.
  SetReportingPolicy(false);
  EXPECT_CALL(*client_, UploadRealtimeReport_(_, _)).Times(0);
  TriggerOnSecurityInterstitialShownEvent();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, event_observer.PassEventArgs().GetList().size());
  Mock::VerifyAndClearExpectations(client_.get());
}

TEST_F(SafeBrowsingPrivateEventRouterTest, PolicyControlOffToOnIsDynamic) {
  SetUpRouters(/*realtime_reporting_enable=*/false);
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnSecurityInterstitialShown::kEventName);
  event_router_->AddEventObserver(&event_observer);

  TriggerOnSecurityInterstitialShownEvent();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, event_observer.PassEventArgs().GetList().size());

  // Now turn on policy.
  SetReportingPolicy(true);
  EXPECT_CALL(*client_, UploadRealtimeReport_(_, _)).Times(1);
  TriggerOnSecurityInterstitialShownEvent();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, event_observer.PassEventArgs().GetList().size());
  Mock::VerifyAndClearExpectations(client_.get());
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestUnauthorizedOnReuseDetected) {
  SetUpRouters(/*realtime_reporting_enable=*/true, /*authorized=*/false);
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnPolicySpecifiedPasswordReuseDetected::
          kEventName);
  event_router_->AddEventObserver(&event_observer);

  base::Value report;
  EXPECT_CALL(*client_, UploadRealtimeReport_(_, _)).Times(0);

  TriggerOnPolicySpecifiedPasswordReuseDetectedEvent();
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(client_.get());
  EXPECT_EQ(base::Value::Type::NONE, report.type());
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestUnauthorizedOnPasswordChanged) {
  SetUpRouters(/*realtime_reporting_enable=*/true, /*authorized=*/false);
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnPolicySpecifiedPasswordChanged::kEventName);
  event_router_->AddEventObserver(&event_observer);

  base::Value report;
  EXPECT_CALL(*client_, UploadRealtimeReport_(_, _)).Times(0);

  TriggerOnPolicySpecifiedPasswordChangedEvent();
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(client_.get());
  EXPECT_EQ(base::Value::Type::NONE, report.type());
}

TEST_F(SafeBrowsingPrivateEventRouterTest,
       TestUnauthorizedOnDangerousDownloadOpened) {
  SetUpRouters(/*realtime_reporting_enable=*/true, /*authorized=*/false);
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnDangerousDownloadOpened::kEventName);
  event_router_->AddEventObserver(&event_observer);

  base::Value report;
  EXPECT_CALL(*client_, UploadRealtimeReport_(_, _)).Times(0);

  TriggerOnDangerousDownloadOpenedEvent();
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(client_.get());
  EXPECT_EQ(base::Value::Type::NONE, report.type());
}

TEST_F(SafeBrowsingPrivateEventRouterTest,
       TestUnauthorizedOnSecurityInterstitialProceeded) {
  SetUpRouters(/*realtime_reporting_enable=*/true, /*authorized=*/false);
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnSecurityInterstitialProceeded::kEventName);
  event_router_->AddEventObserver(&event_observer);

  base::Value report;
  EXPECT_CALL(*client_, UploadRealtimeReport_(_, _)).Times(0);

  TriggerOnSecurityInterstitialProceededEvent();
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(client_.get());
  EXPECT_EQ(base::Value::Type::NONE, report.type());
}

TEST_F(SafeBrowsingPrivateEventRouterTest,
       TestUnauthorizedOnSecurityInterstitialShown) {
  SetUpRouters(/*realtime_reporting_enable=*/true, /*authorized=*/false);
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnSecurityInterstitialShown::kEventName);
  event_router_->AddEventObserver(&event_observer);

  base::Value report;
  EXPECT_CALL(*client_, UploadRealtimeReport_(_, _)).Times(0);

  TriggerOnSecurityInterstitialShownEvent();
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(client_.get());
  EXPECT_EQ(base::Value::Type::NONE, report.type());
}

TEST_F(SafeBrowsingPrivateEventRouterTest,
       TestUnauthorizedOnDangerousDownloadWarning) {
  SetUpRouters(/*realtime_reporting_enable=*/true, /*authorized=*/false);
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnDangerousDownloadOpened::kEventName);
  event_router_->AddEventObserver(&event_observer);

  base::Value report;
  EXPECT_CALL(*client_, UploadRealtimeReport_(_, _)).Times(0);

  TriggerOnDangerousDownloadWarningEvent();
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(client_.get());
  EXPECT_EQ(base::Value::Type::NONE, report.type());
}

TEST_F(SafeBrowsingPrivateEventRouterTest,
       TestUnauthorizedOnDangerousDownloadWarningBypass) {
  SetUpRouters(/*realtime_reporting_enable=*/true, /*authorized=*/false);
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnDangerousDownloadOpened::kEventName);
  event_router_->AddEventObserver(&event_observer);

  base::Value report;
  EXPECT_CALL(*client_, UploadRealtimeReport_(_, _)).Times(0);

  TriggerOnDangerousDownloadWarningEventBypass();
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(client_.get());
  EXPECT_EQ(base::Value::Type::NONE, report.type());
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestOnSensitiveDataEvent) {
  SetUpRouters(/*realtime_reporting_enable=*/true, /*authorized=*/true);

  base::Value report;
  EXPECT_CALL(*client_, UploadRealtimeReport_(_, _))
      .WillOnce(CaptureArg(&report));

  TriggerOnSensitiveDataEvent();
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(client_.get());
  EXPECT_EQ(base::Value::Type::DICTIONARY, report.type());
  base::Value* event_list =
      report.FindKey(policy::RealtimeReportingJobConfiguration::kEventListKey);
  ASSERT_NE(nullptr, event_list);
  EXPECT_EQ(base::Value::Type::LIST, event_list->type());
  base::Value::ListView mutable_list = event_list->GetList();
  ASSERT_EQ(1, (int)mutable_list.size());
  base::Value wrapper = std::move(mutable_list[0]);
  EXPECT_EQ(base::Value::Type::DICTIONARY, wrapper.type());
  base::Value* event =
      wrapper.FindKey(SafeBrowsingPrivateEventRouter::kKeySensitiveDataEvent);
  ASSERT_NE(nullptr, event);

  EXPECT_EQ(12345, *event->FindIntKey(
                       SafeBrowsingPrivateEventRouter::kKeyContentSize));
  EXPECT_EQ("text/plain", *event->FindStringKey(
                              SafeBrowsingPrivateEventRouter::kKeyContentType));
  EXPECT_EQ("sha256_of_data",
            *event->FindStringKey(
                SafeBrowsingPrivateEventRouter::kKeyDownloadDigestSha256));
  EXPECT_EQ(
      "sensitive_data.txt",
      *event->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyFileName));
  EXPECT_EQ("FILE_UPLOAD",
            *event->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyTrigger));

  base::Value* triggered_rule_info =
      event->FindKey(SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleInfo);
  ASSERT_NE(nullptr, triggered_rule_info);
  ASSERT_EQ(1u, triggered_rule_info->GetList().size());
  base::Value triggered_rule = std::move(triggered_rule_info->GetList()[0]);
  EXPECT_EQ(12345, triggered_rule.FindIntKey(
                       SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleId));
  EXPECT_EQ(3, triggered_rule.FindIntKey(
                   SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleAction));
  EXPECT_EQ("fake rule",
            *triggered_rule.FindStringKey(
                SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleName));
  EXPECT_EQ("fake resource name",
            *triggered_rule.FindStringKey(
                SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleResourceName));
  EXPECT_EQ("fake severity",
            *triggered_rule.FindStringKey(
                SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleSeverity));

  base::Value* matched_detectors = triggered_rule.FindKey(
      SafeBrowsingPrivateEventRouter::kKeyMatchedDetectors);
  ASSERT_NE(nullptr, matched_detectors);
  ASSERT_EQ(2u, matched_detectors->GetList().size());
  base::Value detector = std::move(matched_detectors->GetList()[0]);
  EXPECT_EQ("fake id",
            *detector.FindStringKey(
                SafeBrowsingPrivateEventRouter::kKeyMatchedDetectorId));
  EXPECT_EQ("fake type",
            *detector.FindStringKey(
                SafeBrowsingPrivateEventRouter::kKeyMatchedDetectorType));
  EXPECT_EQ("fake name",
            *detector.FindStringKey(
                SafeBrowsingPrivateEventRouter::kKeyMatchedDetectorName));
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestOnUnscannedFileEvent) {
  SetUpRouters(/*realtime_reporting_enable=*/true, /*authorized=*/true);

  base::Value report;
  EXPECT_CALL(*client_, UploadRealtimeReport_(_, _))
      .WillOnce(CaptureArg(&report));

  TriggerOnUnscannedFileEvent();
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(client_.get());
  EXPECT_EQ(base::Value::Type::DICTIONARY, report.type());
  base::Value* event_list =
      report.FindKey(policy::RealtimeReportingJobConfiguration::kEventListKey);
  ASSERT_NE(nullptr, event_list);
  EXPECT_EQ(base::Value::Type::LIST, event_list->type());
  base::Value::ListView mutable_list = event_list->GetList();
  ASSERT_EQ(1, (int)mutable_list.size());
  base::Value wrapper = std::move(mutable_list[0]);
  EXPECT_EQ(base::Value::Type::DICTIONARY, wrapper.type());
  base::Value* event =
      wrapper.FindKey(SafeBrowsingPrivateEventRouter::kKeyUnscannedFileEvent);
  ASSERT_NE(nullptr, event);

  EXPECT_EQ(12345, *event->FindIntKey(
                       SafeBrowsingPrivateEventRouter::kKeyContentSize));
  EXPECT_EQ("text/plain", *event->FindStringKey(
                              SafeBrowsingPrivateEventRouter::kKeyContentType));
  EXPECT_EQ("sha256_of_data",
            *event->FindStringKey(
                SafeBrowsingPrivateEventRouter::kKeyDownloadDigestSha256));
  EXPECT_EQ(
      "sensitive_data.txt",
      *event->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyFileName));
  EXPECT_EQ("FILE_DOWNLOAD",
            *event->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyTrigger));
  EXPECT_EQ("filePasswordProtected",
            *event->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyReason));
}

// Tests to make sure the feature flag and policy control real-time reporting
// as expected.  The parameter for these tests is a tuple of bools:
//
//   bool: whether the feature flag is enabled.
//   bool: whether the browser is manageable.
//   bool: whether the policy is enabled.
//   bool: whether the server has authorized this browser instance.
class SafeBrowsingIsRealtimeReportingEnabledTest
    : public SafeBrowsingPrivateEventRouterTest,
      public testing::WithParamInterface<
          testing::tuple<bool, bool, bool, bool>> {
 public:
  SafeBrowsingIsRealtimeReportingEnabledTest()
      : is_feature_flag_enabled_(testing::get<0>(GetParam())),
        is_manageable_(testing::get<1>(GetParam())),
        is_policy_enabled_(testing::get<2>(GetParam())),
        is_authorized_(testing::get<3>(GetParam())) {
    if (is_feature_flag_enabled_) {
      scoped_feature_list_.InitAndEnableFeature(
          SafeBrowsingPrivateEventRouter::kRealtimeReportingFeature);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          SafeBrowsingPrivateEventRouter::kRealtimeReportingFeature);
    }

    // In chrome branded desktop builds, the browser is always manageable.
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING) && !defined(OS_CHROMEOS)
    if (is_manageable_) {
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          switches::kEnableChromeBrowserCloudManagement);
    }
#endif

    TestingBrowserProcess::GetGlobal()->local_state()->SetBoolean(
        prefs::kUnsafeEventsReportingEnabled, is_policy_enabled_);

#if defined(OS_CHROMEOS)
    auto user_manager = std::make_unique<chromeos::FakeChromeUserManager>();
    const AccountId account_id(
        AccountId::FromUserEmail(profile_->GetProfileUserName()));
    const user_manager::User* user = user_manager->AddUserWithAffiliation(
        account_id, /*is_affiliated=*/is_manageable_);
    chromeos::ProfileHelper::Get()->SetUserToProfileMappingForTesting(user,
                                                                      profile_);
    user_manager->UserLoggedIn(account_id, user->username_hash(),
                               /*browser_restart=*/false,
                               /*is_child=*/false);
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));
    profile_->ScopedCrosSettingsTestHelper()
        ->InstallAttributes()
        ->SetCloudManaged("domain.com", "device_id");
#endif
  }

  bool should_init() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !defined(OS_CHROMEOS)
    return is_feature_flag_enabled_;
#else
    return is_feature_flag_enabled_ && is_manageable_;
#endif
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  const bool is_feature_flag_enabled_;
  const bool is_manageable_;
  const bool is_policy_enabled_;
  const bool is_authorized_;

#if defined(OS_CHROMEOS)
 private:
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
#endif
};

TEST_P(SafeBrowsingIsRealtimeReportingEnabledTest,
       ShouldInitRealtimeReportingClient) {
  EXPECT_EQ(
      should_init(),
      SafeBrowsingPrivateEventRouter::ShouldInitRealtimeReportingClient());
}

TEST_P(SafeBrowsingIsRealtimeReportingEnabledTest, CheckRealtimeReport) {
  // In production, the router won't actually be authorized unless it was
  // initialized.  The second argument to SetUpRouters() takes this into
  // account.
  SetUpRouters(is_policy_enabled_, should_init() && is_authorized_);
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnPolicySpecifiedPasswordChanged::kEventName);
  event_router_->AddEventObserver(&event_observer);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !defined(OS_CHROMEOS)
  bool should_report =
      is_feature_flag_enabled_ && is_policy_enabled_ && is_authorized_;
#else
  bool should_report = is_feature_flag_enabled_ && is_manageable_ &&
                       is_policy_enabled_ && is_authorized_;
#endif

  if (should_report) {
    EXPECT_CALL(*client_, UploadRealtimeReport_(_, _)).Times(1);
  } else if (client_) {
    // Because the test will crate a |client_| object when the policy is
    // set, even if the feature flag or other conditions indicate that
    // reports should not be sent, it is possible that the pointer is not
    // null. In this case, make sure UploadRealtimeReport() is not called.
    EXPECT_CALL(*client_, UploadRealtimeReport_(_, _)).Times(0);
  }

  TriggerOnPolicySpecifiedPasswordChangedEvent();
  base::RunLoop().RunUntilIdle();

  // Asser the triggered actually did fire.
  EXPECT_EQ(1u, event_observer.PassEventArgs().GetList().size());

  // Make sure UploadRealtimeReport was called the expected number of times.
  if (client_)
    Mock::VerifyAndClearExpectations(client_.get());
}

INSTANTIATE_TEST_SUITE_P(All,
                         SafeBrowsingIsRealtimeReportingEnabledTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));

}  // namespace extensions
