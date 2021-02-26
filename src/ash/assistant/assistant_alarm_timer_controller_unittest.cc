// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_alarm_timer_controller_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/assistant/assistant_controller_impl.h"
#include "ash/assistant/assistant_notification_controller_impl.h"
#include "ash/assistant/model/assistant_alarm_timer_model.h"
#include "ash/assistant/model/assistant_alarm_timer_model_observer.h"
#include "ash/assistant/model/assistant_notification_model.h"
#include "ash/assistant/model/assistant_notification_model_observer.h"
#include "ash/assistant/test/assistant_ash_test_base.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/macros.h"
#include "base/test/icu_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/services/assistant/public/cpp/assistant_notification.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

using chromeos::assistant::AssistantNotification;
using chromeos::assistant::AssistantNotificationButton;
using chromeos::assistant::AssistantNotificationPriority;

// Constants.
constexpr char kClientId[] = "assistant/timer<timer-id>";
constexpr char kTimerId[] = "<timer-id>";

// Mocks -----------------------------------------------------------------------

class MockAssistantAlarmTimerModelObserver
    : public testing::NiceMock<AssistantAlarmTimerModelObserver> {
 public:
  MOCK_METHOD(void, OnTimerAdded, (const AssistantTimer&), (override));

  MOCK_METHOD(void, OnTimerUpdated, (const AssistantTimer&), (override));
};

// Test Structs ----------------------------------------------------------------

// Represents a test instruction to advance the tick of the mock clock and
// assert an |expected_string|.
typedef struct {
  base::TimeDelta advance_clock;
  std::string expected_string;
} TestTick;

// Represents a |locale| specific test case containing assertions to be made at
// various |ticks| of the mock clock.
typedef struct {
  std::string locale;
  std::vector<TestTick> ticks;
} I18nTestCase;

// Timer Events ----------------------------------------------------------------

class TimerEvent {
 public:
  TimerEvent& WithLabel(const std::string& label) {
    timer_->label = label;
    return *this;
  }

  TimerEvent& WithCreationTime(base::Optional<base::Time> creation_time) {
    timer_->creation_time = creation_time;
    return *this;
  }

  TimerEvent& WithOriginalDuration(base::TimeDelta original_duration) {
    timer_->original_duration = original_duration;
    return *this;
  }

  TimerEvent& WithRemainingTime(base::TimeDelta remaining_time) {
    timer_->fire_time = base::Time::Now() + remaining_time;
    timer_->remaining_time = remaining_time;
    return *this;
  }

 protected:
  TimerEvent(const std::string& id, AssistantTimerState state)
      : timer_(std::make_unique<AssistantTimer>()) {
    timer_->id = id;
    timer_->state = state;
    timer_->fire_time = base::Time::Now();
  }

  ~TimerEvent() {
    std::vector<AssistantTimerPtr> timers;
    timers.push_back(std::move(timer_));
    AssistantAlarmTimerController::Get()->OnTimerStateChanged(
        std::move(timers));
  }

 private:
  AssistantTimerPtr timer_;
};

class FireTimer : public TimerEvent {
 public:
  explicit FireTimer(const std::string& id)
      : TimerEvent(id, AssistantTimerState::kFired) {}
};

class PauseTimer : public TimerEvent {
 public:
  explicit PauseTimer(const std::string& id)
      : TimerEvent(id, AssistantTimerState::kPaused) {}
};

class ScheduleTimer : public TimerEvent {
 public:
  explicit ScheduleTimer(const std::string& id)
      : TimerEvent(id, AssistantTimerState::kScheduled) {}
};

// Expectations ----------------------------------------------------------------

class ExpectedNotification {
 public:
  ExpectedNotification() = default;
  ExpectedNotification(const ExpectedNotification&) = delete;
  ExpectedNotification& operator=(const ExpectedNotification&) = delete;
  ~ExpectedNotification() = default;

  friend std::ostream& operator<<(std::ostream& os,
                                  const ExpectedNotification& notif) {
    if (notif.action_url_.has_value())
      os << "\naction_url: '" << notif.action_url_.value() << "'";
    if (notif.client_id_.has_value())
      os << "\nclient_id: '" << notif.client_id_.value() << "'";
    if (notif.is_pinned_.has_value())
      os << "\nis_pinned: '" << notif.is_pinned_.value() << "'";
    if (notif.message_.has_value())
      os << "\nmessage: '" << notif.message_.value() << "'";
    if (notif.priority_.has_value())
      os << "\npriority: '" << static_cast<int>(notif.priority_.value()) << "'";
    if (notif.remove_on_click_.has_value())
      os << "\nremove_on_click: '" << notif.remove_on_click_.value() << "'";
    if (notif.renotify_.has_value())
      os << "\nrenotify: '" << notif.renotify_.value() << "'";
    if (notif.title_.has_value())
      os << "\ntitle: '" << notif.title_.value() << "'";
    return os;
  }

  bool operator==(const AssistantNotification& notification) const {
    return notification.action_url ==
               action_url_.value_or(notification.action_url) &&
           notification.client_id ==
               client_id_.value_or(notification.client_id) &&
           notification.is_pinned ==
               is_pinned_.value_or(notification.is_pinned) &&
           notification.message == message_.value_or(notification.message) &&
           notification.priority == priority_.value_or(notification.priority) &&
           notification.remove_on_click ==
               remove_on_click_.value_or(notification.remove_on_click) &&
           notification.renotify == renotify_.value_or(notification.renotify) &&
           notification.title == title_.value_or(notification.title);
  }

  ExpectedNotification& WithActionUrl(const GURL& action_url) {
    action_url_ = action_url;
    return *this;
  }

  ExpectedNotification& WithClientId(const std::string& client_id) {
    client_id_ = client_id;
    return *this;
  }

  ExpectedNotification& WithIsPinned(bool is_pinned) {
    is_pinned_ = is_pinned;
    return *this;
  }

  ExpectedNotification& WithMessage(const std::string& message) {
    message_ = message;
    return *this;
  }

  ExpectedNotification& WithPriority(AssistantNotificationPriority priority) {
    priority_ = priority;
    return *this;
  }

  ExpectedNotification& WithRemoveOnClick(bool remove_on_click) {
    remove_on_click_ = remove_on_click;
    return *this;
  }

  ExpectedNotification& WithRenotify(bool renotify) {
    renotify_ = renotify;
    return *this;
  }

  ExpectedNotification& WithTitle(const std::string& title) {
    title_ = title;
    return *this;
  }

  ExpectedNotification& WithTitleId(int title_id) {
    return WithTitle(l10n_util::GetStringUTF8(title_id));
  }

 private:
  base::Optional<GURL> action_url_;
  base::Optional<std::string> client_id_;
  base::Optional<bool> is_pinned_;
  base::Optional<std::string> message_;
  base::Optional<AssistantNotificationPriority> priority_;
  base::Optional<bool> remove_on_click_;
  base::Optional<bool> renotify_;
  base::Optional<std::string> title_;
};

class ExpectedButton {
 public:
  ExpectedButton() = default;
  ExpectedButton(const ExpectedButton&) = delete;
  ExpectedButton& operator=(const ExpectedButton&) = delete;
  ~ExpectedButton() = default;

  friend std::ostream& operator<<(std::ostream& os, const ExpectedButton& btr) {
    if (btr.action_url_.has_value())
      os << "\naction_url: '" << btr.action_url_.value() << "'";
    if (btr.label_.has_value())
      os << "\nlabel: '" << btr.label_.value() << "'";
    if (btr.remove_notification_on_click_.has_value()) {
      os << "\nremove_notification_on_click: '"
         << btr.remove_notification_on_click_.value() << "'";
    }
    return os;
  }

  bool operator==(const AssistantNotificationButton& button) const {
    return button.action_url == action_url_.value_or(button.action_url) &&
           button.label == label_.value_or(button.label) &&
           button.remove_notification_on_click ==
               remove_notification_on_click_.value_or(
                   button.remove_notification_on_click);
  }

  ExpectedButton& WithActionUrl(const GURL& action_url) {
    action_url_ = action_url;
    return *this;
  }

  ExpectedButton& WithLabel(int message_id) {
    label_ = l10n_util::GetStringUTF8(message_id);
    return *this;
  }

  ExpectedButton& WithRemoveNotificationOnClick(
      bool remove_notification_on_click) {
    remove_notification_on_click_ = remove_notification_on_click;
    return *this;
  }

 private:
  base::Optional<GURL> action_url_;
  base::Optional<std::string> label_;
  base::Optional<bool> remove_notification_on_click_;
};

// ScopedNotificationModelObserver ---------------------------------------------

class ScopedNotificationModelObserver
    : public AssistantNotificationModelObserver {
 public:
  ScopedNotificationModelObserver() {
    Shell::Get()
        ->assistant_controller()
        ->notification_controller()
        ->model()
        ->AddObserver(this);
  }

  ~ScopedNotificationModelObserver() override {
    Shell::Get()
        ->assistant_controller()
        ->notification_controller()
        ->model()
        ->RemoveObserver(this);
  }

  // AssistantNotificationModelObserver:
  void OnNotificationAdded(const AssistantNotification& notification) override {
    last_notification_ = notification;
  }

  void OnNotificationUpdated(
      const AssistantNotification& notification) override {
    last_notification_ = notification;
  }

  const AssistantNotification& last_notification() const {
    return last_notification_;
  }

 private:
  AssistantNotification last_notification_;

  DISALLOW_COPY_AND_ASSIGN(ScopedNotificationModelObserver);
};

}  // namespace

// AssistantAlarmTimerControllerTest -------------------------------------------

class AssistantAlarmTimerControllerTest : public AssistantAshTestBase {
 protected:
  AssistantAlarmTimerControllerTest()
      : AssistantAshTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  ~AssistantAlarmTimerControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AssistantAshTestBase::SetUp();
    feature_list_.InitAndDisableFeature(
        chromeos::assistant::features::kAssistantTimersV2);
  }

  void TearDown() override {
    controller()->OnTimerStateChanged({});
    AssistantAshTestBase::TearDown();
  }

  // Advances the clock by |time_delta| and waits for the next timer update.
  // NOTE: If |time_delta| is zero, this method is a no-op.
  void AdvanceClockAndWaitForTimerUpdate(base::TimeDelta time_delta) {
    if (time_delta.is_zero())
      return;

    task_environment()->AdvanceClock(time_delta);

    MockAssistantAlarmTimerModelObserver observer;
    controller()->GetModel()->AddObserver(&observer);

    base::RunLoop run_loop;
    EXPECT_CALL(observer, OnTimerUpdated)
        .WillOnce(testing::Invoke([&](const AssistantTimer& timer) {
          run_loop.QuitClosure().Run();
        }));
    run_loop.Run();

    controller()->GetModel()->RemoveObserver(&observer);
  }

  AssistantAlarmTimerController* controller() {
    return AssistantAlarmTimerController::Get();
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(AssistantAlarmTimerControllerTest);
};

// Tests -----------------------------------------------------------------------

// Tests that creation time is properly respected/defaulted when adding a timer.
TEST_F(AssistantAlarmTimerControllerTest, AddedTimersShouldHaveCreationTime) {
  MockAssistantAlarmTimerModelObserver mock;
  controller()->GetModel()->AddObserver(&mock);

  // If unspecified, |creation_time| is expected to be now.
  base::Time creation_time = base::Time::Now();
  EXPECT_CALL(mock, OnTimerAdded)
      .WillOnce(testing::Invoke([&](const AssistantTimer& timer) {
        EXPECT_EQ(creation_time, timer.creation_time.value());
      }));

  // Schedule a timer w/o specifying |creation_time|.
  ScheduleTimer{kTimerId};

  // Reset for the next test case.
  controller()->OnTimerStateChanged({});
  testing::Mock::VerifyAndClearExpectations(&mock);

  // If specified, |creation_time| should be respected.
  creation_time -= base::TimeDelta::FromMinutes(1);
  EXPECT_CALL(mock, OnTimerAdded)
      .WillOnce(testing::Invoke([&](const AssistantTimer& timer) {
        EXPECT_EQ(creation_time, timer.creation_time.value());
      }));

  // Schedule a timer w/ specified |creation _time|.
  ScheduleTimer(kTimerId).WithCreationTime(creation_time);

  controller()->GetModel()->RemoveObserver(&mock);
}

// Tests that creation time is properly respected/carried forward when updating
// a timer.
TEST_F(AssistantAlarmTimerControllerTest, UpdatedTimersShouldHaveCreationTime) {
  MockAssistantAlarmTimerModelObserver mock;
  controller()->GetModel()->AddObserver(&mock);

  base::Time creation_time = base::Time::Now();

  // Schedule a timer w/ specified |creation_time|.
  ScheduleTimer(kTimerId).WithCreationTime(creation_time);

  // Advance clock.
  AdvanceClockAndWaitForTimerUpdate(base::TimeDelta::FromMinutes(1));

  // If unspecified, |creation_time| should carry forward on update.
  EXPECT_CALL(mock, OnTimerUpdated)
      .WillOnce(testing::Invoke([&](const AssistantTimer& timer) {
        EXPECT_NE(creation_time, base::Time::Now());
        EXPECT_EQ(creation_time, timer.creation_time.value());
      }));

  // Update timer w/o specifying |creation_time|.
  ScheduleTimer{kTimerId};

  // Reset for the next test case.
  testing::Mock::VerifyAndClearExpectations(&mock);

  // If specified, |creation_time| should be respected.
  creation_time += base::TimeDelta::FromHours(1);
  EXPECT_CALL(mock, OnTimerUpdated)
      .WillOnce(testing::Invoke([&](const AssistantTimer& timer) {
        EXPECT_EQ(creation_time, timer.creation_time.value());
      }));

  // Update timer w/ specified |creation_time|.
  ScheduleTimer(kTimerId).WithCreationTime(creation_time);

  controller()->GetModel()->RemoveObserver(&mock);
}

// Tests that a notification is added for a timer and has the expected title.
// NOTE: This test is only applicable to timers v1.
TEST_F(AssistantAlarmTimerControllerTest, TimerNotificationHasExpectedTitle) {
  ASSERT_FALSE(chromeos::assistant::features::IsTimersV2Enabled());

  // Observe notifications.
  ScopedNotificationModelObserver observer;

  // Fire a timer.
  FireTimer{kTimerId};

  // We expect that a notification exists w/ an internationalized title.
  EXPECT_EQ(ExpectedNotification().WithClientId(kClientId).WithTitleId(
                IDS_ASSISTANT_TIMER_NOTIFICATION_TITLE),
            observer.last_notification());
}

// Tests that a notification is added for a timer and has the expected title at
// various states in its lifecycle.
// NOTE: This test is only applicable to timers v2.
TEST_F(AssistantAlarmTimerControllerTest, TimerNotificationHasExpectedTitleV2) {
  // Enable timers v2.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      chromeos::assistant::features::kAssistantTimersV2);
  ASSERT_TRUE(chromeos::assistant::features::IsTimersV2Enabled());

  // We're going to run our test over a few locales to ensure i18n compliance.
  std::vector<I18nTestCase> i18n_test_cases;

  // We'll test in English (United States).
  i18n_test_cases.push_back({
      /*locale=*/"en_US",
      /*ticks=*/
      {
          {base::TimeDelta(), "1:01:01"},
          {base::TimeDelta::FromHours(1), "1:01"},
          {base::TimeDelta::FromMinutes(1), "0:01"},
          {base::TimeDelta::FromSeconds(1), "0:00"},
          {base::TimeDelta::FromSeconds(1), "-0:01"},
          {base::TimeDelta::FromMinutes(1), "-1:01"},
          {base::TimeDelta::FromHours(1), "-1:01:01"},
      },
  });

  // We'll also test in Slovenian (Slovenia).
  i18n_test_cases.push_back({
      /*locale=*/"sl_SI",
      /*ticks=*/
      {
          {base::TimeDelta(), "1.01.01"},
          {base::TimeDelta::FromHours(1), "1.01"},
          {base::TimeDelta::FromMinutes(1), "0.01"},
          {base::TimeDelta::FromSeconds(1), "0.00"},
          {base::TimeDelta::FromSeconds(1), "-0.01"},
          {base::TimeDelta::FromMinutes(1), "-1.01"},
          {base::TimeDelta::FromHours(1), "-1.01.01"},
      },
  });

  // Run all of our internationalized test cases.
  for (auto& i18n_test_case : i18n_test_cases) {
    base::test::ScopedRestoreICUDefaultLocale locale(i18n_test_case.locale);

    // Observe notifications.
    ScopedNotificationModelObserver observer;

    // Schedule a timer.
    ScheduleTimer(kTimerId).WithRemainingTime(base::TimeDelta::FromHours(1) +
                                              base::TimeDelta::FromMinutes(1) +
                                              base::TimeDelta::FromSeconds(1));

    // Run each tick of the clock in the test.
    for (auto& tick : i18n_test_case.ticks) {
      // Advance clock to next tick.
      AdvanceClockAndWaitForTimerUpdate(tick.advance_clock);

      // Make assertions about the notification.
      EXPECT_EQ(ExpectedNotification().WithClientId(kClientId).WithTitle(
                    tick.expected_string),
                observer.last_notification());
    }
  }
}

// Tests that a notification is added for a timer and has the expected message
// at various states in its lifecycle.
// NOTE: This test is only applicable to timers v1.
TEST_F(AssistantAlarmTimerControllerTest, TimerNotificationHasExpectedMessage) {
  ASSERT_FALSE(chromeos::assistant::features::IsTimersV2Enabled());

  // We're going to run our test over a few locales to ensure i18n compliance.
  std::vector<I18nTestCase> i18n_test_cases;

  // We'll test in English (United States).
  i18n_test_cases.push_back({
      /*locale=*/"en_US",
      /*ticks=*/
      {
          {base::TimeDelta(), "0:00"},
          {base::TimeDelta::FromSeconds(1), "-0:01"},
          {base::TimeDelta::FromMinutes(1), "-1:01"},
          {base::TimeDelta::FromHours(1), "-1:01:01"},
      },
  });

  // We'll also test in Slovenian (Slovenia).
  i18n_test_cases.push_back({
      /*locale=*/"sl_SI",
      /*ticks=*/
      {
          {base::TimeDelta(), "0.00"},
          {base::TimeDelta::FromSeconds(1), "-0.01"},
          {base::TimeDelta::FromMinutes(1), "-1.01"},
          {base::TimeDelta::FromHours(1), "-1.01.01"},
      },
  });

  // Run all of our internationalized test cases.
  for (auto& i18n_test_case : i18n_test_cases) {
    base::test::ScopedRestoreICUDefaultLocale locale(i18n_test_case.locale);

    // Observe notifications.
    ScopedNotificationModelObserver observer;

    // Fire a timer.
    FireTimer{kTimerId};

    // Run each tick of the clock in the test.
    for (auto& tick : i18n_test_case.ticks) {
      // Advance clock to next tick.
      AdvanceClockAndWaitForTimerUpdate(tick.advance_clock);

      // Make assertions about the notification.
      EXPECT_EQ(ExpectedNotification().WithClientId(kClientId).WithMessage(
                    tick.expected_string),
                observer.last_notification());
    }
  }
}

// TODO(dmblack): Add another locale after string translation.
// Tests that a notification is added for a timer and has the expected message.
// NOTE: This test is only applicable to timers v2.
TEST_F(AssistantAlarmTimerControllerTest,
       TimerNotificationHasExpectedMessageV2) {
  // Enable timers v2.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      chromeos::assistant::features::kAssistantTimersV2);
  ASSERT_TRUE(chromeos::assistant::features::IsTimersV2Enabled());

  constexpr char kEmptyLabel[] = "";
  constexpr base::TimeDelta kOneSec = base::TimeDelta::FromSeconds(1);
  constexpr base::TimeDelta kOneMin = base::TimeDelta::FromMinutes(1);
  constexpr base::TimeDelta kOneHour = base::TimeDelta::FromHours(1);

  // We'll verify the message of our notification with various timers.
  typedef struct {
    base::TimeDelta original_duration;
    std::string label;
    std::string expected_message_when_scheduled;
    std::string expected_message_when_fired;
  } TestTimer;

  // We're going to run our test over a few locales to ensure i18n compliance.
  typedef struct {
    std::string locale;
    std::vector<TestTimer> timers;
  } I18nTestCase;

  std::vector<I18nTestCase> i18n_test_cases;

  // We'll test in English (United States).
  i18n_test_cases.push_back({
      /*locale=*/"en_US",
      /*timers=*/
      {
          {kOneSec, kEmptyLabel, "1s timer", "Time's up"},
          {kOneSec, "Eggs", "1s timer · Eggs", "Time's up · Eggs"},
          {kOneSec + kOneMin, kEmptyLabel, "1m 1s timer", "Time's up"},
          {kOneSec + kOneMin, "Eggs", "1m 1s timer · Eggs", "Time's up · Eggs"},
          {kOneSec + kOneMin + kOneHour, kEmptyLabel, "1h 1m 1s timer",
           "Time's up"},
          {kOneSec + kOneMin + kOneHour, "Eggs", "1h 1m 1s timer · Eggs",
           "Time's up · Eggs"},
      },
  });

  // Run all of our internationalized test cases.
  for (auto& i18n_test_case : i18n_test_cases) {
    base::test::ScopedRestoreICUDefaultLocale locale(i18n_test_case.locale);

    // Observe notifications.
    ScopedNotificationModelObserver observer;

    // Run each timer in the test.
    for (auto& timer : i18n_test_case.timers) {
      // Schedule timer.
      ScheduleTimer(kTimerId)
          .WithLabel(timer.label)
          .WithOriginalDuration(timer.original_duration);

      // Make assertions about the notification.
      EXPECT_EQ(ExpectedNotification().WithClientId(kClientId).WithMessage(
                    timer.expected_message_when_scheduled),
                observer.last_notification());

      // Fire timer.
      FireTimer(kTimerId)
          .WithLabel(timer.label)
          .WithOriginalDuration(timer.original_duration);

      // Make assertions about the notification.
      EXPECT_EQ(ExpectedNotification().WithClientId(kClientId).WithMessage(
                    timer.expected_message_when_fired),
                observer.last_notification());
    }
  }
}

// Tests that a notification is added for a timer and has the expected action.
// NOTE: This test is only applicable to timers v1.
TEST_F(AssistantAlarmTimerControllerTest, TimerNotificationHasExpectedAction) {
  ASSERT_FALSE(chromeos::assistant::features::IsTimersV2Enabled());

  // Observe notifications.
  ScopedNotificationModelObserver observer;

  // Fire a timer.
  FireTimer{kTimerId};

  // We expect that the notification action will cause removal of the timer.
  EXPECT_EQ(
      ExpectedNotification().WithClientId(kClientId).WithActionUrl(
          assistant::util::CreateAlarmTimerDeepLink(
              assistant::util::AlarmTimerAction::kRemoveAlarmOrTimer, kTimerId)
              .value()),
      observer.last_notification());
}

// Tests that a notification is added for a timer and has the expected action.
// NOTE: This test is only applicable to timers v2.
TEST_F(AssistantAlarmTimerControllerTest,
       TimerNotificationHasExpectedActionV2) {
  // Enable timers v2.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      chromeos::assistant::features::kAssistantTimersV2);
  ASSERT_TRUE(chromeos::assistant::features::IsTimersV2Enabled());

  // Observe notifications.
  ScopedNotificationModelObserver observer;

  // Schedule a timer.
  ScheduleTimer{kTimerId};

  // We expect that the notification action will result in a no-op on click.
  EXPECT_EQ(
      ExpectedNotification().WithClientId(kClientId).WithActionUrl(GURL()),
      observer.last_notification());
}

// Tests that a notification is added when a timer is fired and has the expected
// buttons.
// NOTE: This test is only applicable to timers v1.
TEST_F(AssistantAlarmTimerControllerTest, TimerNotificationHasExpectedButtons) {
  ASSERT_FALSE(chromeos::assistant::features::IsTimersV2Enabled());

  // Observe notifications.
  ScopedNotificationModelObserver observer;

  // Fire a timer.
  FireTimer{kTimerId};

  // We expect the timer notification to have two buttons.
  ASSERT_EQ(2u, observer.last_notification().buttons.size());

  // We expect a "STOP" button which will remove the timer.
  EXPECT_EQ(ExpectedButton()
                .WithLabel(IDS_ASSISTANT_TIMER_NOTIFICATION_STOP_BUTTON)
                .WithActionUrl(
                    assistant::util::CreateAlarmTimerDeepLink(
                        assistant::util::AlarmTimerAction::kRemoveAlarmOrTimer,
                        kTimerId)
                        .value())
                .WithRemoveNotificationOnClick(true),
            observer.last_notification().buttons.at(0));

  // We expect an "ADD 1 MIN" button which will add time to the timer.
  EXPECT_EQ(
      ExpectedButton()
          .WithLabel(IDS_ASSISTANT_TIMER_NOTIFICATION_ADD_1_MIN_BUTTON)
          .WithActionUrl(assistant::util::CreateAlarmTimerDeepLink(
                             assistant::util::AlarmTimerAction::kAddTimeToTimer,
                             kTimerId, base::TimeDelta::FromMinutes(1))
                             .value())
          .WithRemoveNotificationOnClick(true),
      observer.last_notification().buttons.at(1));
}

// Tests that a notification is added for a timer and has the expected buttons
// at each state in its lifecycle.
// NOTE: This test is only applicable to timers v2.
TEST_F(AssistantAlarmTimerControllerTest,
       TimerNotificationHasExpectedButtonsV2) {
  // Enable timers v2.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      chromeos::assistant::features::kAssistantTimersV2);
  ASSERT_TRUE(chromeos::assistant::features::IsTimersV2Enabled());

  // Observe notifications.
  ScopedNotificationModelObserver observer;

  constexpr base::TimeDelta kTimeRemaining = base::TimeDelta::FromMinutes(1);

  // Schedule a timer.
  ScheduleTimer(kTimerId).WithRemainingTime(kTimeRemaining);

  // We expect the timer notification to have two buttons.
  ASSERT_EQ(2u, observer.last_notification().buttons.size());

  // We expect a "PAUSE" button which will pause the timer.
  EXPECT_EQ(
      ExpectedButton()
          .WithLabel(IDS_ASSISTANT_TIMER_NOTIFICATION_PAUSE_BUTTON)
          .WithActionUrl(
              assistant::util::CreateAlarmTimerDeepLink(
                  assistant::util::AlarmTimerAction::kPauseTimer, kTimerId)
                  .value())
          .WithRemoveNotificationOnClick(false),
      observer.last_notification().buttons.at(0));

  // We expect a "CANCEL" button which will remove the timer.
  EXPECT_EQ(ExpectedButton()
                .WithLabel(IDS_ASSISTANT_TIMER_NOTIFICATION_CANCEL_BUTTON)
                .WithActionUrl(
                    assistant::util::CreateAlarmTimerDeepLink(
                        assistant::util::AlarmTimerAction::kRemoveAlarmOrTimer,
                        kTimerId)
                        .value())
                .WithRemoveNotificationOnClick(true),
            observer.last_notification().buttons.at(1));

  // Pause the timer.
  PauseTimer(kTimerId).WithRemainingTime(kTimeRemaining);

  // We expect the timer notification to have two buttons.
  ASSERT_EQ(2u, observer.last_notification().buttons.size());

  // We expect a "RESUME" button which will resume the timer.
  EXPECT_EQ(
      ExpectedButton()
          .WithLabel(IDS_ASSISTANT_TIMER_NOTIFICATION_RESUME_BUTTON)
          .WithActionUrl(
              assistant::util::CreateAlarmTimerDeepLink(
                  assistant::util::AlarmTimerAction::kResumeTimer, kTimerId)
                  .value())
          .WithRemoveNotificationOnClick(false),
      observer.last_notification().buttons.at(0));

  // We expect a "CANCEL" button which will remove the timer.
  EXPECT_EQ(ExpectedButton()
                .WithLabel(IDS_ASSISTANT_TIMER_NOTIFICATION_CANCEL_BUTTON)
                .WithActionUrl(
                    assistant::util::CreateAlarmTimerDeepLink(
                        assistant::util::AlarmTimerAction::kRemoveAlarmOrTimer,
                        kTimerId)
                        .value())
                .WithRemoveNotificationOnClick(true),
            observer.last_notification().buttons.at(1));

  // Fire the timer.
  FireTimer{kTimerId};

  // We expect the timer notification to have two buttons.
  ASSERT_EQ(2u, observer.last_notification().buttons.size());

  // We expect a "STOP" button which will remove the timer.
  EXPECT_EQ(ExpectedButton()
                .WithLabel(IDS_ASSISTANT_TIMER_NOTIFICATION_STOP_BUTTON)
                .WithActionUrl(
                    assistant::util::CreateAlarmTimerDeepLink(
                        assistant::util::AlarmTimerAction::kRemoveAlarmOrTimer,
                        kTimerId)
                        .value())
                .WithRemoveNotificationOnClick(true),
            observer.last_notification().buttons.at(0));

  // We expect an "ADD 1 MIN" button which will add time to the timer.
  EXPECT_EQ(
      ExpectedButton()
          .WithLabel(IDS_ASSISTANT_TIMER_NOTIFICATION_ADD_1_MIN_BUTTON)
          .WithActionUrl(assistant::util::CreateAlarmTimerDeepLink(
                             assistant::util::AlarmTimerAction::kAddTimeToTimer,
                             kTimerId, base::TimeDelta::FromMinutes(1))
                             .value())
          .WithRemoveNotificationOnClick(false),
      observer.last_notification().buttons.at(1));
}

// Tests that a notification is added for a timer and has the expected value to
// cause removal on click.
// NOTE: This test is only applicable to timers v1.
TEST_F(AssistantAlarmTimerControllerTest,
       TimerNotificationHasExpectedRemoveOnClick) {
  ASSERT_FALSE(chromeos::assistant::features::IsTimersV2Enabled());

  // Observe notifications.
  ScopedNotificationModelObserver observer;

  // Fire a timer.
  FireTimer{kTimerId};

  // Make assertions about the notification.
  EXPECT_EQ(
      ExpectedNotification().WithClientId(kClientId).WithRemoveOnClick(true),
      observer.last_notification());
}

// Tests that a notification is added for a timer and has the expected value to
// *not* cause removal on click.
// Note: This test is only applicable to timers v2.
TEST_F(AssistantAlarmTimerControllerTest,
       TimerNotificationHasExpectedRemoveOnClickV2) {
  // Enable timers v2.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      chromeos::assistant::features::kAssistantTimersV2);
  ASSERT_TRUE(chromeos::assistant::features::IsTimersV2Enabled());

  // Observe notifications.
  ScopedNotificationModelObserver observer;

  // Schedule a timer.
  ScheduleTimer{kTimerId};

  // Make assertions about the notification.
  EXPECT_EQ(
      ExpectedNotification().WithClientId(kClientId).WithRemoveOnClick(false),
      observer.last_notification());
}

// Tests that a notification is added for a timer and has the expected value to
// *not* cause renotification.
// NOTE: This test is only applicable to timers v1.
TEST_F(AssistantAlarmTimerControllerTest,
       TimerNotificationHasExpectedRenotify) {
  ASSERT_FALSE(chromeos::assistant::features::IsTimersV2Enabled());

  // Observe notifications.
  ScopedNotificationModelObserver observer;

  // Fire a timer.
  FireTimer{kTimerId};

  // Make assertions about the notification.
  EXPECT_EQ(ExpectedNotification().WithClientId(kClientId).WithRenotify(false),
            observer.last_notification());
}

// Tests that a notification is added for a timer and has the expected value to
// cause renotification.
// Note: This test is only applicable to timers v2.
TEST_F(AssistantAlarmTimerControllerTest,
       TimerNotificationHasExpectedRenotifyV2) {
  // Enable timers v2.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      chromeos::assistant::features::kAssistantTimersV2);
  ASSERT_TRUE(chromeos::assistant::features::IsTimersV2Enabled());

  // Observe notifications.
  ScopedNotificationModelObserver observer;

  // Schedule a timer.
  ScheduleTimer{kTimerId};

  // Make assertions about the notification.
  EXPECT_EQ(ExpectedNotification().WithClientId(kClientId).WithRenotify(false),
            observer.last_notification());

  // Fire the timer.
  FireTimer{kTimerId};

  // Make assertions about the notification.
  EXPECT_EQ(ExpectedNotification().WithClientId(kClientId).WithRenotify(true),
            observer.last_notification());
}

// Tests that a notification is added for a timer and has the expected priority.
// NOTE: This test is only applicable to timers v1.
TEST_F(AssistantAlarmTimerControllerTest,
       TimerNotificationHasExpectedPriority) {
  ASSERT_FALSE(chromeos::assistant::features::IsTimersV2Enabled());

  // Observe notifications.
  ScopedNotificationModelObserver notification_model_observer;

  // Fire a timer.
  FireTimer{kTimerId};

  // Make assertions about the notification.
  EXPECT_EQ(ExpectedNotification().WithClientId(kClientId).WithPriority(
                AssistantNotificationPriority::kHigh),
            notification_model_observer.last_notification());
}

// Tests that a notification is added for a timer and has the expected priority
// at various stages of its lifecycle.
// NOTE: This test is only applicable to timers v2.
TEST_F(AssistantAlarmTimerControllerTest,
       TimerNotificationHasExpectedPriorityV2) {
  // Enable timers v2.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      chromeos::assistant::features::kAssistantTimersV2);
  ASSERT_TRUE(chromeos::assistant::features::IsTimersV2Enabled());

  // Observe notifications.
  ScopedNotificationModelObserver notification_model_observer;

  // Schedule a timer.
  ScheduleTimer(kTimerId).WithRemainingTime(base::TimeDelta::FromSeconds(10));

  // Make assertions about the notification.
  EXPECT_EQ(ExpectedNotification().WithClientId(kClientId).WithPriority(
                AssistantNotificationPriority::kDefault),
            notification_model_observer.last_notification());

  // Advance the clock.
  // NOTE: Six seconds is the threshold for popping up our notification.
  AdvanceClockAndWaitForTimerUpdate(base::TimeDelta::FromSeconds(6));

  // Make assertions about the notification.
  EXPECT_EQ(ExpectedNotification().WithClientId(kClientId).WithPriority(
                AssistantNotificationPriority::kLow),
            notification_model_observer.last_notification());

  // Fire the timer.
  FireTimer{kTimerId};

  // Make assertions about the notification.
  EXPECT_EQ(ExpectedNotification().WithClientId(kClientId).WithPriority(
                AssistantNotificationPriority::kHigh),
            notification_model_observer.last_notification());
}

// Tests that a notification is added for a timer and is not pinned.
// NOTE: This test is only applicable to timers v1.
TEST_F(AssistantAlarmTimerControllerTest, TimerNotificationIsNotPinned) {
  ASSERT_FALSE(chromeos::assistant::features::IsTimersV2Enabled());

  // Observe notifications.
  ScopedNotificationModelObserver observer;

  // Fire a timer.
  FireTimer{kTimerId};

  // Make assertions about the notification.
  EXPECT_EQ(ExpectedNotification().WithClientId(kClientId).WithIsPinned(false),
            observer.last_notification());
}

// Tests that a notification is added for a timer and is pinned.
// NOTE: This test is only applicable to timers v2.
TEST_F(AssistantAlarmTimerControllerTest, TimerNotificationIsPinned) {
  // Enable timers v2.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      chromeos::assistant::features::kAssistantTimersV2);
  ASSERT_TRUE(chromeos::assistant::features::IsTimersV2Enabled());

  // Observe notifications.
  ScopedNotificationModelObserver observer;

  // Schedule a timer.
  ScheduleTimer{kTimerId};

  // Make assertions about the notification.
  EXPECT_EQ(ExpectedNotification().WithClientId(kClientId).WithIsPinned(true),
            observer.last_notification());
}

}  // namespace ash
