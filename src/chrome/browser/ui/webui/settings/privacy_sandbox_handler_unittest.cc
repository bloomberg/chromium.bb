// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/privacy_sandbox_handler.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/privacy_sandbox/canonical_topic.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "components/privacy_sandbox/privacy_sandbox_test_util.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

constexpr char kCallbackId1[] = "test-callback-id";
constexpr char kCallbackId2[] = "test-callback-id-2";

class MockPrivacySandboxService : public PrivacySandboxService {
 public:
  MOCK_METHOD(void,
              GetFledgeJoiningEtldPlusOneForDisplay,
              (base::OnceCallback<void(std::vector<std::string>)>),
              (override));
  MOCK_METHOD(std::vector<std::string>,
              GetBlockedFledgeJoiningTopFramesForDisplay,
              (),
              (const override));
  MOCK_METHOD(void,
              SetFledgeJoiningAllowed,
              ((const std::string&), bool),
              (const override));
  MOCK_METHOD(std::vector<privacy_sandbox::CanonicalTopic>,
              GetCurrentTopTopics,
              (),
              (const override));
  MOCK_METHOD(std::vector<privacy_sandbox::CanonicalTopic>,
              GetBlockedTopics,
              (),
              (const override));
  MOCK_METHOD(void,
              SetTopicAllowed,
              (privacy_sandbox::CanonicalTopic, bool),
              (override));
};

std::unique_ptr<KeyedService> BuildMockPrivacySandboxService(
    content::BrowserContext* context) {
  return std::make_unique<::testing::StrictMock<MockPrivacySandboxService>>();
}

// Confirms that the |floc_id| dictionary provided matches the current FLoC
// information for |profile|.
void ValidateFlocId(const base::Value* floc_id, Profile* profile) {
  auto* privacy_sandbox_service =
      PrivacySandboxServiceFactory::GetForProfile(profile);

  ASSERT_TRUE(floc_id->is_dict());
  EXPECT_EQ(
      base::UTF16ToUTF8(privacy_sandbox_service->GetFlocStatusForDisplay()),
      *floc_id->FindStringPath("trialStatus"));
  EXPECT_EQ(base::UTF16ToUTF8(privacy_sandbox_service->GetFlocIdForDisplay()),
            *floc_id->FindStringPath("cohort"));
  EXPECT_EQ(
      base::UTF16ToUTF8(privacy_sandbox_service->GetFlocIdNextUpdateForDisplay(
          base::Time::Now())),
      *floc_id->FindStringPath("nextUpdate"));
  EXPECT_EQ(privacy_sandbox_service->IsFlocIdResettable(),
            floc_id->FindBoolPath("canReset"));
}

void ValidateFledgeInfo(content::TestWebUI* web_ui,
                        std::string expected_callback_id,
                        std::vector<std::string> expected_joining_sites,
                        std::vector<std::string> expected_blocked_sites) {
  const content::TestWebUI::CallData& data = *web_ui->call_data().back();
  EXPECT_EQ(expected_callback_id, data.arg1()->GetString());
  EXPECT_EQ("cr.webUIResponse", data.function_name());
  ASSERT_TRUE(data.arg2()->GetBool());
  ASSERT_TRUE(data.arg3()->is_dict());

  auto* blocked_sites = data.arg3()->FindListKey("blockedSites");
  ASSERT_TRUE(blocked_sites);
  ASSERT_EQ(expected_blocked_sites.size(),
            blocked_sites->GetListDeprecated().size());
  for (size_t i = 0; i < expected_blocked_sites.size(); i++) {
    EXPECT_EQ(expected_blocked_sites[i],
              blocked_sites->GetListDeprecated()[i].GetString());
  }

  auto* joining_sites = data.arg3()->FindListKey("joiningSites");
  ASSERT_TRUE(joining_sites);
  ASSERT_EQ(expected_joining_sites.size(),
            joining_sites->GetListDeprecated().size());
  for (size_t i = 0; i < expected_joining_sites.size(); i++) {
    EXPECT_EQ(expected_joining_sites[i],
              joining_sites->GetListDeprecated()[i].GetString());
  }
}

void ValidateTopicsInfo(
    std::vector<privacy_sandbox::CanonicalTopic> expected_topics,
    base::Value::ConstListView actual_topics) {
  ASSERT_EQ(expected_topics.size(), actual_topics.size());
  for (size_t i = 0; i < expected_topics.size(); i++) {
    const auto& actual_topic = actual_topics[i];
    const auto& expected_topic = expected_topics[i];
    ASSERT_TRUE(actual_topic.is_dict());
    ASSERT_EQ(expected_topic.topic_id(), actual_topic.FindIntKey("topicId"));
    ASSERT_EQ(expected_topic.taxonomy_version(),
              actual_topic.FindIntKey("taxonomyVersion"));
    ASSERT_EQ(expected_topic.GetLocalizedRepresentation(),
              base::UTF8ToUTF16(*actual_topic.FindStringKey("displayString")));
  }
}

}  // namespace

namespace settings {

class PrivacySandboxHandlerTest : public testing::Test {
 public:
  PrivacySandboxHandlerTest()
      : browser_task_environment_(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->set_web_contents(web_contents_.get());
    handler_ = std::make_unique<PrivacySandboxHandler>();
    handler_->set_web_ui(web_ui());
    handler_->AllowJavascript();
    web_ui_->ClearTrackedCalls();
  }

  void TearDown() override {
    handler_->set_web_ui(nullptr);
    handler_.reset();
    web_ui_.reset();
  }

  content::TestWebUI* web_ui() { return web_ui_.get(); }
  PrivacySandboxHandler* handler() { return handler_.get(); }
  TestingProfile* profile() { return &profile_; }
  PrivacySandboxSettings* privacy_sandbox_settings() {
    return PrivacySandboxSettingsFactory::GetForProfile(profile());
  }

 private:
  content::BrowserTaskEnvironment browser_task_environment_;
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> web_contents_ =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<PrivacySandboxHandler> handler_;
};

TEST_F(PrivacySandboxHandlerTest, GetFlocId) {
  base::Value args(base::Value::Type::LIST);
  args.Append(kCallbackId1);
  handler()->HandleGetFlocId(args.GetListDeprecated());

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ(kCallbackId1, data.arg1()->GetString());
  EXPECT_EQ("cr.webUIResponse", data.function_name());
  ASSERT_TRUE(data.arg2()->GetBool());
  ValidateFlocId(data.arg3(), profile());
}

TEST_F(PrivacySandboxHandlerTest, ResetFlocId) {
  // Observers of the PrivacySandboxSettings service should be informed that
  // the FLoC ID was reset.
  privacy_sandbox_test_util::MockPrivacySandboxObserver observer;
  privacy_sandbox_settings()->AddObserver(&observer);
  EXPECT_CALL(observer, OnFlocDataAccessibleSinceUpdated(true));

  base::Value args(base::Value::Type::LIST);
  handler()->HandleResetFlocId(args.GetListDeprecated());

  // Resetting the FLoC ID should also fire the appropriate WebUI listener.
  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIListenerCallback", data.function_name());
  EXPECT_EQ("floc-id-changed", data.arg1()->GetString());
  ValidateFlocId(data.arg2(), profile());
}

class PrivacySandboxHandlerTestMockService : public PrivacySandboxHandlerTest {
 public:
  void SetUp() override {
    PrivacySandboxHandlerTest::SetUp();

    mock_privacy_sandbox_service_ = static_cast<MockPrivacySandboxService*>(
        PrivacySandboxServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&BuildMockPrivacySandboxService)));
  }

  MockPrivacySandboxService* mock_privacy_sandbox_service() {
    return mock_privacy_sandbox_service_;
  }

 private:
  raw_ptr<MockPrivacySandboxService> mock_privacy_sandbox_service_;
};

TEST_F(PrivacySandboxHandlerTestMockService, SetFledgeJoiningAllowed) {
  // Confirm that the handler forward FLEDGE allowed changes to the service.
  const std::string kTestSite = "example.com";
  EXPECT_CALL(*mock_privacy_sandbox_service(),
              SetFledgeJoiningAllowed(kTestSite, true));

  base::Value args(base::Value::Type::LIST);
  args.Append(kTestSite);
  args.Append(true);
  handler()->HandleSetFledgeJoiningAllowed(args.GetListDeprecated());
}

TEST_F(PrivacySandboxHandlerTestMockService, GetFledgeState) {
  // Confirm that FLEDGE state is correctly returned. As FLEDGE state is
  // retrieved async, the handler must also support multiple requests in flight.
  using Callback = base::OnceCallback<void(std::vector<std::string>)>;
  Callback callback_one;
  Callback callback_two;

  EXPECT_CALL(*mock_privacy_sandbox_service(),
              GetFledgeJoiningEtldPlusOneForDisplay(testing::_))
      .Times(2)
      .WillOnce([&](Callback callback) { callback_one = std::move(callback); })
      .WillOnce([&](Callback callback) { callback_two = std::move(callback); });

  base::Value args(base::Value::Type::LIST);
  args.Append(kCallbackId1);
  handler()->HandleGetFledgeState(args.GetListDeprecated());

  args.ClearList();
  args.Append(kCallbackId2);
  handler()->HandleGetFledgeState(args.GetListDeprecated());

  // Provide different sets of information to each request to the FLEDGE
  // backend.
  const std::vector<std::string> kJoiningSites1 = {"e.com", "f.com"};
  const std::vector<std::string> kJoiningSites2 = {"g.com", "h.com"};
  const std::vector<std::string> kBlockedSites1 = {"a.com", "b.com"};
  const std::vector<std::string> kBlockedSites2 = {"c.com", "d.com"};

  EXPECT_CALL(*mock_privacy_sandbox_service(),
              GetBlockedFledgeJoiningTopFramesForDisplay())
      .Times(2)
      .WillOnce(testing::Return(kBlockedSites1))
      .WillOnce(testing::Return(kBlockedSites2));

  std::move(callback_one).Run(kJoiningSites1);
  ValidateFledgeInfo(web_ui(), kCallbackId1, kJoiningSites1, kBlockedSites1);

  std::move(callback_two).Run(kJoiningSites2);
  ValidateFledgeInfo(web_ui(), kCallbackId2, kJoiningSites2, kBlockedSites2);
}

TEST_F(PrivacySandboxHandlerTestMockService, SetTopicAllowed) {
  // Confirm that the handler correctly constructs the CanonicalTopic and
  // passes it to the PrivacySandboxService.
  const privacy_sandbox::CanonicalTopic kTestTopic(
      1, privacy_sandbox::CanonicalTopic::AVAILABLE_TAXONOMY);
  EXPECT_CALL(*mock_privacy_sandbox_service(),
              SetTopicAllowed(kTestTopic, false))
      .Times(1);
  base::Value args(base::Value::Type::LIST);
  args.Append(kTestTopic.topic_id());
  args.Append(kTestTopic.taxonomy_version());
  args.Append(false);
  handler()->HandleSetTopicAllowed(args.GetListDeprecated());
}

TEST_F(PrivacySandboxHandlerTestMockService, GetTopicsState) {
  const std::vector<privacy_sandbox::CanonicalTopic> kBlockedTopics = {
      privacy_sandbox::CanonicalTopic(
          1, privacy_sandbox::CanonicalTopic::AVAILABLE_TAXONOMY),
      privacy_sandbox::CanonicalTopic(
          2, privacy_sandbox::CanonicalTopic::AVAILABLE_TAXONOMY)};
  const std::vector<privacy_sandbox::CanonicalTopic> kTopTopics = {
      privacy_sandbox::CanonicalTopic(
          3, privacy_sandbox::CanonicalTopic::AVAILABLE_TAXONOMY),
      privacy_sandbox::CanonicalTopic(
          4, privacy_sandbox::CanonicalTopic::AVAILABLE_TAXONOMY)};

  EXPECT_CALL(*mock_privacy_sandbox_service(), GetCurrentTopTopics())
      .Times(1)
      .WillOnce(testing::Return(kTopTopics));
  EXPECT_CALL(*mock_privacy_sandbox_service(), GetBlockedTopics())
      .Times(1)
      .WillOnce(testing::Return(kBlockedTopics));

  base::Value args(base::Value::Type::LIST);
  args.Append(kCallbackId1);
  handler()->HandleGetTopicsState(args.GetListDeprecated());

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ(kCallbackId1, data.arg1()->GetString());
  EXPECT_EQ("cr.webUIResponse", data.function_name());
  ASSERT_TRUE(data.arg2()->GetBool());
  ASSERT_TRUE(data.arg3()->is_dict());

  ValidateTopicsInfo(
      kTopTopics, data.arg3()->FindListKey("topTopics")->GetListDeprecated());
  ValidateTopicsInfo(
      kBlockedTopics,
      data.arg3()->FindListKey("blockedTopics")->GetListDeprecated());
}

}  // namespace settings
