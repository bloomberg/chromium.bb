// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/audio/cras_audio_handler.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/time/time.h"
#include "chrome/browser/ui/ash/assistant/assistant_test_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/assistant/test_support/expect_utils.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "chromeos/services/assistant/service.h"
#include "content/public/test/browser_test.h"

namespace chromeos {
namespace assistant {
namespace {

using ::ash::CrasAudioHandler;

// Please remember to set auth token when running in |kProxy| mode.
constexpr auto kMode = FakeS3Mode::kReplay;
// Update this when you introduce breaking changes to existing tests.
constexpr int kVersion = 1;

constexpr int kStartBrightnessPercent = 50;

// Ensures that |value_| is within the range {min_, max_}. If it isn't, this
// will print a nice error message.
#define EXPECT_WITHIN_RANGE(min_, value_, max_)                \
  ({                                                           \
    EXPECT_TRUE(min_ <= value_ && value_ <= max_)              \
        << "Expected " << value_ << " to be within the range " \
        << "{" << min_ << ", " << max_ << "}.";                \
  })

}  // namespace

using chromeos::assistant::test::ExpectResult;

class AssistantBrowserTest : public MixinBasedInProcessBrowserTest {
 public:
  AssistantBrowserTest() {
    // TODO(b/190633242): enable sandbox in browser tests.
    feature_list_.InitAndDisableFeature(
        chromeos::assistant::features::kEnableLibAssistantSandbox);
  }

  AssistantBrowserTest(const AssistantBrowserTest&) = delete;
  AssistantBrowserTest& operator=(const AssistantBrowserTest&) = delete;

  ~AssistantBrowserTest() override = default;

  AssistantTestMixin* tester() { return &tester_; }

  void ShowAssistantUi() {
    if (!tester()->IsVisible())
      tester()->PressAssistantKey();
  }

  void CloseAssistantUi() {
    if (tester()->IsVisible())
      tester()->PressAssistantKey();
  }

  void InitializeBrightness() {
    auto* power_manager = chromeos::PowerManagerClient::Get();
    power_manager::SetBacklightBrightnessRequest request;
    request.set_percent(kStartBrightnessPercent);
    request.set_transition(
        power_manager::SetBacklightBrightnessRequest_Transition_INSTANT);
    request.set_cause(
        power_manager::SetBacklightBrightnessRequest_Cause_USER_REQUEST);
    chromeos::PowerManagerClient::Get()->SetScreenBrightness(request);

    // Wait for the initial value to settle.
    ExpectResult(
        true, base::BindLambdaForTesting([&]() {
          constexpr double kEpsilon = 0.1;
          auto current_brightness = tester()->SyncCall(base::BindOnce(
              &chromeos::PowerManagerClient::GetScreenBrightnessPercent,
              base::Unretained(power_manager)));
          return current_brightness &&
                 std::abs(kStartBrightnessPercent -
                          current_brightness.value()) < kEpsilon;
        }));
  }

  void ExpectBrightnessUp() {
    auto* power_manager = chromeos::PowerManagerClient::Get();
    // Check the brightness changes
    ExpectResult(
        true, base::BindLambdaForTesting([&]() {
          constexpr double kEpsilon = 1;
          auto current_brightness = tester()->SyncCall(base::BindOnce(
              &chromeos::PowerManagerClient::GetScreenBrightnessPercent,
              base::Unretained(power_manager)));

          return current_brightness && (current_brightness.value() -
                                        kStartBrightnessPercent) > kEpsilon;
        }));
  }

  void ExpectBrightnessDown() {
    auto* power_manager = chromeos::PowerManagerClient::Get();
    // Check the brightness changes
    ExpectResult(
        true, base::BindLambdaForTesting([&]() {
          constexpr double kEpsilon = 1;
          auto current_brightness = tester()->SyncCall(base::BindOnce(
              &chromeos::PowerManagerClient::GetScreenBrightnessPercent,
              base::Unretained(power_manager)));

          return current_brightness && (kStartBrightnessPercent -
                                        current_brightness.value()) > kEpsilon;
        }));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  AssistantTestMixin tester_{&mixin_host_, this, embedded_test_server(), kMode,
                             kVersion};
};

IN_PROC_BROWSER_TEST_F(AssistantBrowserTest,
                       ShouldOpenAssistantUiWhenPressingAssistantKey) {
  tester()->StartAssistantAndWaitForReady();

  tester()->PressAssistantKey();

  EXPECT_TRUE(tester()->IsVisible());
}

// TODO(b/184802501): Fix this flaky test.
IN_PROC_BROWSER_TEST_F(AssistantBrowserTest,
                       DISABLED_ShouldDisplayTextResponse) {
  tester()->StartAssistantAndWaitForReady();

  ShowAssistantUi();

  tester()->SendTextQuery("test");
  tester()->ExpectAnyOfTheseTextResponses({
      "No one told me there would be a test",
      "You're coming in loud and clear",
      "debug OK",
      "I can assure you, this thing's on",
      "Is this thing on?",
  });
}

// Flaky. See https://crbug.com/1196560.
IN_PROC_BROWSER_TEST_F(AssistantBrowserTest,
                       DISABLED_ShouldDisplayCardResponse) {
  tester()->StartAssistantAndWaitForReady();

  ShowAssistantUi();

  ASSERT_TRUE(tester()->IsVisible());

  tester()->SendTextQuery("What is the highest mountain in the world?");
  tester()->ExpectCardResponse("Mount Everest");
}

IN_PROC_BROWSER_TEST_F(AssistantBrowserTest, ShouldTurnUpVolume) {
  tester()->StartAssistantAndWaitForReady();

  ShowAssistantUi();

  ASSERT_TRUE(tester()->IsVisible());

  auto* cras = CrasAudioHandler::Get();
  constexpr int kStartVolumePercent = 50;
  cras->SetOutputVolumePercent(kStartVolumePercent);
  EXPECT_EQ(kStartVolumePercent, cras->GetOutputVolumePercent());

  tester()->SendTextQuery("turn up volume");

  ExpectResult(true, base::BindRepeating(
                         [](CrasAudioHandler* cras) {
                           return cras->GetOutputVolumePercent() >
                                  kStartVolumePercent;
                         },
                         cras));
}

IN_PROC_BROWSER_TEST_F(AssistantBrowserTest, ShouldTurnDownVolume) {
  tester()->StartAssistantAndWaitForReady();

  ShowAssistantUi();

  ASSERT_TRUE(tester()->IsVisible());

  auto* cras = CrasAudioHandler::Get();
  constexpr int kStartVolumePercent = 50;
  cras->SetOutputVolumePercent(kStartVolumePercent);
  EXPECT_EQ(kStartVolumePercent, cras->GetOutputVolumePercent());

  tester()->SendTextQuery("turn down volume");

  ExpectResult(true, base::BindRepeating(
                         [](CrasAudioHandler* cras) {
                           return cras->GetOutputVolumePercent() <
                                  kStartVolumePercent;
                         },
                         cras));
}

IN_PROC_BROWSER_TEST_F(AssistantBrowserTest, ShouldTurnUpBrightness) {
  tester()->StartAssistantAndWaitForReady();

  ShowAssistantUi();

  ASSERT_TRUE(tester()->IsVisible());

  InitializeBrightness();

  tester()->SendTextQuery("turn up brightness");

  ExpectBrightnessUp();
}

IN_PROC_BROWSER_TEST_F(AssistantBrowserTest, ShouldTurnDownBrightness) {
  tester()->StartAssistantAndWaitForReady();

  ShowAssistantUi();

  ASSERT_TRUE(tester()->IsVisible());

  InitializeBrightness();

  tester()->SendTextQuery("turn down brightness");

  ExpectBrightnessDown();
}

IN_PROC_BROWSER_TEST_F(AssistantBrowserTest,
                       ShouldPuntWhenChangingUnsupportedSetting) {
  tester()->StartAssistantAndWaitForReady();

  ShowAssistantUi();

  ASSERT_TRUE(tester()->IsVisible());

  tester()->SendTextQuery("enable night mode");

  tester()->ExpectTextResponse("Night Mode isn't available on your device");
}

// TODO(crbug.com/1112278): Disabled because it's flaky.
IN_PROC_BROWSER_TEST_F(AssistantBrowserTest,
                       DISABLED_ShouldShowSingleErrorOnNetworkDown) {
  tester()->StartAssistantAndWaitForReady();

  ShowAssistantUi();

  ASSERT_TRUE(tester()->IsVisible());

  tester()->DisableFakeS3Server();

  base::RunLoop().RunUntilIdle();

  tester()->SendTextQuery("Is this thing on?");

  tester()->ExpectErrorResponse(
      "Something went wrong. Try again in a few seconds");

  // Make sure no further changes happen to the view hierarchy.
  tester()->ExpectNoChange(base::Seconds(1));

  // This is necessary to prevent a UserInitiatedVoicelessActivity from
  // blocking test harness teardown while we wait on assistant to finish
  // the interaction.
  CloseAssistantUi();
}

}  // namespace assistant
}  // namespace chromeos
