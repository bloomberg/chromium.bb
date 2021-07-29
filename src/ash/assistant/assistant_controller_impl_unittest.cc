// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_controller_impl.h"

#include <map>
#include <memory>
#include <string>

#include "ash/assistant/test/assistant_ash_test_base.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/public/cpp/assistant/controller/assistant_controller_observer.h"
#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "base/scoped_observation.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

namespace {

// MockAssistantControllerObserver ---------------------------------------------

class MockAssistantControllerObserver : public AssistantControllerObserver {
 public:
  MockAssistantControllerObserver() = default;
  ~MockAssistantControllerObserver() override = default;

  // AssistantControllerObserver:
  MOCK_METHOD(void, OnAssistantControllerConstructed, (), (override));

  MOCK_METHOD(void, OnAssistantControllerDestroying, (), (override));

  MOCK_METHOD(void, OnAssistantReady, (), (override));

  MOCK_METHOD(void,
              OnDeepLinkReceived,
              (assistant::util::DeepLinkType type,
               (const std::map<std::string, std::string>& params)),
              (override));

  MOCK_METHOD(void,
              OnOpeningUrl,
              (const GURL& url, bool in_background, bool from_server),
              (override));

  MOCK_METHOD(void,
              OnUrlOpened,
              (const GURL& url, bool from_server),
              (override));
};

class MockAssistantUiModelObserver : public AssistantUiModelObserver {
 public:
  MockAssistantUiModelObserver() = default;
  ~MockAssistantUiModelObserver() override = default;

  MOCK_METHOD(void,
              OnUiVisibilityChanged,
              (AssistantVisibility new_visibility,
               AssistantVisibility old_visibility,
               absl::optional<AssistantEntryPoint> entry_point,
               absl::optional<AssistantExitPoint> exit_point),
              (override));
};

// MockNewWindowDelegate -------------------------------------------------------

class MockNewWindowDelegate : public testing::NiceMock<TestNewWindowDelegate> {
 public:
  // TestNewWindowDelegate:
  MOCK_METHOD(void,
              NewTabWithUrl,
              (const GURL& url, bool from_user_interaction),
              (override));

  MOCK_METHOD(void,
              OpenFeedbackPage,
              (NewWindowDelegate::FeedbackSource source,
               const std::string& description_template),
              (override));
};

// AssistantControllerImplTest -------------------------------------------------

class AssistantControllerImplTest : public AssistantAshTestBase {
 public:
  AssistantControllerImplTest() {
    auto delegate = std::make_unique<MockNewWindowDelegate>();
    new_window_delegate_ = delegate.get();
    delegate_provider_ =
        std::make_unique<TestNewWindowDelegateProvider>(std::move(delegate));
  }

  AssistantController* controller() { return AssistantController::Get(); }
  MockNewWindowDelegate& new_window_delegate() { return *new_window_delegate_; }
  const AssistantUiModel* ui_model() {
    return AssistantUiController::Get()->GetModel();
  }

 private:
  MockNewWindowDelegate* new_window_delegate_;
  std::unique_ptr<TestNewWindowDelegateProvider> delegate_provider_;
};

}  // namespace

// Tests -----------------------------------------------------------------------

// Tests that AssistantController observers are notified of deep link received.
TEST_F(AssistantControllerImplTest, NotifiesDeepLinkReceived) {
  testing::NiceMock<MockAssistantControllerObserver> controller_observer_mock;
  base::ScopedObservation<AssistantController, AssistantControllerObserver>
      scoped_controller_obs{&controller_observer_mock};
  scoped_controller_obs.Observe(controller());

  EXPECT_CALL(controller_observer_mock, OnDeepLinkReceived)
      .WillOnce(
          testing::Invoke([](assistant::util::DeepLinkType type,
                             const std::map<std::string, std::string>& params) {
            EXPECT_EQ(assistant::util::DeepLinkType::kQuery, type);
            EXPECT_EQ("weather",
                      assistant::util::GetDeepLinkParam(
                          params, assistant::util::DeepLinkParam::kQuery)
                          .value());
          }));

  controller()->OpenUrl(
      assistant::util::CreateAssistantQueryDeepLink("weather"));
}

// Tests that AssistantController observers are notified of URLs opening and
// having been opened. Note that it is important that these events be notified
// before and after the URL is actually opened respectively.
TEST_F(AssistantControllerImplTest, NotifiesOpeningUrlAndUrlOpened) {
  testing::NiceMock<MockAssistantControllerObserver> controller_observer_mock;
  base::ScopedObservation<AssistantController, AssistantControllerObserver>
      scoped_controller_obs{&controller_observer_mock};
  scoped_controller_obs.Observe(controller());

  // Enforce ordering of events.
  testing::InSequence sequence;

  EXPECT_CALL(controller_observer_mock, OnOpeningUrl)
      .WillOnce(testing::Invoke(
          [](const GURL& url, bool in_background, bool from_server) {
            EXPECT_EQ(GURL("https://g.co/"), url);
            EXPECT_TRUE(in_background);
            EXPECT_TRUE(from_server);
          }));

  EXPECT_CALL(new_window_delegate(), NewTabWithUrl)
      .WillOnce([](const GURL& url, bool from_user_interaction) {
        EXPECT_EQ(GURL("https://g.co/"), url);
        EXPECT_TRUE(from_user_interaction);
      });

  EXPECT_CALL(controller_observer_mock, OnUrlOpened)
      .WillOnce(testing::Invoke([](const GURL& url, bool from_server) {
        EXPECT_EQ(GURL("https://g.co/"), url);
        EXPECT_TRUE(from_server);
      }));

  controller()->OpenUrl(GURL("https://g.co/"), /*in_background=*/true,
                        /*from_server=*/true);
}

TEST_F(AssistantControllerImplTest, OpensFeedbackPageForFeedbackDeeplink) {
  testing::NiceMock<MockAssistantControllerObserver> controller_observer_mock;
  base::ScopedObservation<AssistantController, AssistantControllerObserver>
      scoped_controller_obs{&controller_observer_mock};
  scoped_controller_obs.Observe(controller());

  EXPECT_CALL(controller_observer_mock, OnDeepLinkReceived)
      .WillOnce(
          testing::Invoke([](assistant::util::DeepLinkType type,
                             const std::map<std::string, std::string>& params) {
            EXPECT_EQ(assistant::util::DeepLinkType::kFeedback, type);
            std::map<std::string, std::string> expected_params;
            EXPECT_EQ(params, expected_params);
          }));

  EXPECT_CALL(new_window_delegate(), OpenFeedbackPage)
      .WillOnce([](NewWindowDelegate::FeedbackSource source,
                   const std::string& description_template) {
        EXPECT_EQ(NewWindowDelegate::FeedbackSource::kFeedbackSourceAssistant,
                  source);
        EXPECT_EQ(std::string(), description_template);
      });

  controller()->OpenUrl(GURL("googleassistant://send-feedback"),
                        /*in_background=*/false, /*from_server=*/true);
}

TEST_F(AssistantControllerImplTest, ClosesAssistantUiForFeedbackDeeplink) {
  ShowAssistantUi();

  testing::NiceMock<MockAssistantUiModelObserver> ui_model_observer_mock;
  ui_model()->AddObserver(&ui_model_observer_mock);

  EXPECT_CALL(ui_model_observer_mock, OnUiVisibilityChanged)
      .WillOnce([](AssistantVisibility new_visibility,
                   AssistantVisibility old_visibility,
                   absl::optional<AssistantEntryPoint> entry_point,
                   absl::optional<AssistantExitPoint> exit_point) {
        EXPECT_EQ(old_visibility, AssistantVisibility::kVisible);
        EXPECT_EQ(new_visibility, AssistantVisibility::kClosed);
        EXPECT_FALSE(entry_point.has_value());
        EXPECT_EQ(exit_point.value(), AssistantExitPoint::kUnspecified);
      });

  controller()->OpenUrl(GURL("googleassistant://send-feedback"),
                        /*in_background=*/false, /*from_server=*/true);

  ui_model()->RemoveObserver(&ui_model_observer_mock);
}

}  // namespace ash
