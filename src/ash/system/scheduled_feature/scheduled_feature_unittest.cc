// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <limits>
#include <memory>
#include <sstream>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/session/session_types.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/system/geolocation/geolocation_controller.h"
#include "ash/system/scheduled_feature/scheduled_feature.h"
#include "ash/system/time/time_of_day.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test_shell_delegate.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/strings/pattern.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "components/prefs/pref_service.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace ash {

namespace {

constexpr char kUser1Email[] = "user1@featuredschedule";
constexpr char kUser2Email[] = "user2@featuredschedule";

enum AmPm { kAM, kPM };

Geoposition CreateGeoposition(double latitude,
                              double longitude,
                              base::Time timestamp) {
  Geoposition position;
  position.latitude = latitude;
  position.longitude = longitude;
  position.status = Geoposition::STATUS_OK;
  position.accuracy = 10;
  position.timestamp = timestamp;
  return position;
}

class TestScheduledFeature : public ScheduledFeature {
 public:
  TestScheduledFeature(const std::string prefs_path_enabled,
                       const std::string prefs_path_schedule_type,
                       const std::string prefs_path_custom_start_time,
                       const std::string prefs_path_custom_end_time)
      : ScheduledFeature(prefs_path_enabled,
                         prefs_path_schedule_type,
                         prefs_path_custom_start_time,
                         prefs_path_custom_end_time) {}
  TestScheduledFeature(const TestScheduledFeature& other) = delete;
  TestScheduledFeature& operator=(const TestScheduledFeature& rhs) = delete;
  ~TestScheduledFeature() override {}

  const char* GetFeatureName() const override { return "TestFeature"; }
};

// An observer class to GeolocationController which updates sunset and sunrise
// time.
class GeolocationControllerObserver : public GeolocationController::Observer {
 public:
  GeolocationControllerObserver() = default;

  GeolocationControllerObserver(const GeolocationControllerObserver&) = delete;
  GeolocationControllerObserver& operator=(
      const GeolocationControllerObserver&) = delete;

  ~GeolocationControllerObserver() override = default;

  // TODO(crbug.com/1269915): Add `sunset_` and `sunrise_` and update their
  // values when receiving the new position.
  void OnGeopositionChanged(bool possible_change_in_timezone) override {
    position_received_num_++;
    possible_change_in_timezone_ = possible_change_in_timezone;
  }

  int position_received_num() const { return position_received_num_; }
  bool possible_change_in_timezone() const {
    return possible_change_in_timezone_;
  }

 private:
  // The number of times a new position is received.
  int position_received_num_ = 0;
  bool possible_change_in_timezone_ = false;
};

// A fake implementation of GeolocationController that doesn't perform any
// actual geoposition requests.
class FakeGeolocationController : public GeolocationController {
 public:
  FakeGeolocationController(base::SimpleTestClock* test_clock,
                            std::unique_ptr<base::MockOneShotTimer> mock_timer)
      : GeolocationController(/*url_context_getter=*/nullptr) {
    SetTimerForTesting(std::move(mock_timer));
    SetClockForTesting(test_clock);
  }

  FakeGeolocationController(const FakeGeolocationController&) = delete;
  FakeGeolocationController& operator=(const FakeGeolocationController&) =
      delete;

  ~FakeGeolocationController() override = default;

  void set_position_to_send(const Geoposition& position) {
    position_to_send_ = position;
  }

 protected:
  // GeolocationController:
  void RequestGeoposition() override {
    OnGeoposition(position_to_send_, /*server_error=*/false, base::TimeDelta());
  }

 private:
  // The position to send to the observer by the controller the next time
  // `OnGeoposition()` is invoked.
  Geoposition position_to_send_;
};

class ScheduledFeatureTest : public NoSessionAshTestBase {
 public:
  ScheduledFeatureTest() = default;
  ScheduledFeatureTest(const ScheduledFeatureTest& other) = delete;
  ScheduledFeatureTest& operator=(const ScheduledFeatureTest& rhs) = delete;
  ~ScheduledFeatureTest() override = default;

  PrefService* user1_pref_service() {
    return Shell::Get()->session_controller()->GetUserPrefServiceForUser(
        AccountId::FromUserEmail(kUser1Email));
  }

  PrefService* user2_pref_service() {
    return Shell::Get()->session_controller()->GetUserPrefServiceForUser(
        AccountId::FromUserEmail(kUser2Email));
  }

  ScheduledFeature* feature() { return feature_.get(); }

  // AshTestBase:
  void SetUp() override {
    NoSessionAshTestBase::SetUp();

    CreateTestUserSessions();

    // Simulate user 1 login.
    SimulateNewUserFirstLogin(kUser1Email);

    std::unique_ptr<base::MockOneShotTimer> mock_timer =
        std::make_unique<base::MockOneShotTimer>();
    mock_timer_ptr_ = mock_timer.get();
    geolocation_controller_ = std::make_unique<FakeGeolocationController>(
        &test_clock_, std::move(mock_timer));

    // Use user prefs of NightLight, which is an example of ScheduledFeature.
    feature_ = std::make_unique<TestScheduledFeature>(
        prefs::kNightLightEnabled, prefs::kNightLightScheduleType,
        prefs::kNightLightCustomStartTime, prefs::kNightLightCustomEndTime);
    ASSERT_FALSE(feature_->GetEnabled());

    feature_->SetClockForTesting(&test_clock_);
    feature_->OnActiveUserPrefServiceChanged(
        Shell::Get()->session_controller()->GetActivePrefService());

    base::Time now;
    EXPECT_TRUE(base::Time::FromUTCString("23 Dec 2021 12:00:00", &now));
    test_clock_.SetNow(now);
  }

  void TearDown() override {
    geolocation_controller_.reset();
    feature_.reset();
    NoSessionAshTestBase::TearDown();
  }

  void CreateTestUserSessions() {
    auto* session_controller_client = GetSessionControllerClient();
    session_controller_client->Reset();
    session_controller_client->AddUserSession(kUser1Email);
    session_controller_client->AddUserSession(kUser2Email);
  }

  void SwitchActiveUser(const std::string& email) {
    GetSessionControllerClient()->SwitchActiveUser(
        AccountId::FromUserEmail(email));
  }

  bool GetEnabled() { return feature_->GetEnabled(); }
  ScheduledFeature::ScheduleType GetScheduleType() {
    return feature_->GetScheduleType();
  }

  void SetFeatureEnabled(bool enabled) { feature_->SetEnabled(enabled); }
  void SetScheduleType(ScheduledFeature::ScheduleType type) {
    feature_->SetScheduleType(type);
  }

  // Convenience function for constructing a TimeOfDay object for exact hours
  // during the day. |hour| is between 1 and 12.
  TimeOfDay MakeTimeOfDay(int hour, AmPm am_pm) {
    DCHECK_GE(hour, 1);
    DCHECK_LE(hour, 12);

    if (am_pm == kAM) {
      hour %= 12;
    } else {
      if (hour != 12)
        hour += 12;
      hour %= 24;
    }

    return TimeOfDay(hour * 60).SetClock(&test_clock_);
  }

 protected:
  std::unique_ptr<TestScheduledFeature> feature_;
  std::unique_ptr<FakeGeolocationController> geolocation_controller_;
  base::SimpleTestClock test_clock_;
  base::MockOneShotTimer* mock_timer_ptr_;
};

// Tests that switching users retrieves the feature settings for the active
// user's prefs.
TEST_F(ScheduledFeatureTest, UserSwitchAndSettingsPersistence) {
  // Start with user1 logged in and update to sunset-to-sunrise schedule type.
  const std::string kScheduleTypePrefString = prefs::kNightLightScheduleType;
  ScheduledFeature::ScheduleType user1_schedule_type =
      ScheduledFeature::ScheduleType::kSunsetToSunrise;
  feature()->SetScheduleType(user1_schedule_type);
  EXPECT_EQ(user1_schedule_type, GetScheduleType());
  EXPECT_EQ(user1_schedule_type,
            user1_pref_service()->GetInteger(kScheduleTypePrefString));

  // Switch to user 2, and set to custom schedule type.
  SwitchActiveUser(kUser2Email);

  ScheduledFeature::ScheduleType user2_schedule_type =
      ScheduledFeature::ScheduleType::kCustom;
  user2_pref_service()->SetInteger(kScheduleTypePrefString,
                                   user2_schedule_type);
  EXPECT_EQ(user2_schedule_type, GetScheduleType());
  EXPECT_EQ(user1_schedule_type,
            user1_pref_service()->GetInteger(kScheduleTypePrefString));

  // Switch back to user 1, to find feature schedule type is restored to
  // sunset-to-sunrise.
  SwitchActiveUser(kUser1Email);
  EXPECT_EQ(user1_schedule_type, GetScheduleType());
}

// Tests transitioning from kNone to kCustom and back to kNone schedule
// types.
TEST_F(ScheduledFeatureTest, ScheduleNoneToCustomTransition) {
  // Now is 6:00 PM.
  test_clock_.SetNow(MakeTimeOfDay(6, AmPm::kPM).ToTimeToday());
  SetFeatureEnabled(false);
  feature()->SetScheduleType(ScheduledFeature::ScheduleType::kNone);
  // Start time is at 3:00 PM and end time is at 8:00 PM.
  feature()->SetCustomStartTime(MakeTimeOfDay(3, AmPm::kPM));
  feature()->SetCustomEndTime(MakeTimeOfDay(8, AmPm::kPM));

  //      15:00         18:00         20:00
  // <----- + ----------- + ----------- + ----->
  //        |             |             |
  //      start          now           end
  //
  // Even though "Now" is inside the feature interval, nothing should
  // change, since the schedule type is "none".
  EXPECT_FALSE(GetEnabled());

  // Now change the schedule type to custom, the feature should turn on
  // immediately, and the timer should be running with a delay of exactly 2
  // hours scheduling the end.
  feature()->SetScheduleType(ScheduledFeature::ScheduleType::kCustom);
  EXPECT_TRUE(GetEnabled());
  EXPECT_TRUE(feature()->timer()->IsRunning());
  EXPECT_EQ(base::Hours(2), feature()->timer()->GetCurrentDelay());

  // If the user changes the schedule type to "none", the feature status
  // should not change, but the timer should not be running.
  feature()->SetScheduleType(ScheduledFeature::ScheduleType::kNone);
  EXPECT_TRUE(GetEnabled());
  EXPECT_FALSE(feature()->timer()->IsRunning());
}

// Tests what happens when the time now reaches the end of the feature
// interval when the feature mode is on.
TEST_F(ScheduledFeatureTest, TestCustomScheduleReachingEndTime) {
  test_clock_.SetNow(MakeTimeOfDay(6, AmPm::kPM).ToTimeToday());
  feature()->SetCustomStartTime(MakeTimeOfDay(3, AmPm::kPM));
  feature()->SetCustomEndTime(MakeTimeOfDay(8, AmPm::kPM));
  feature()->SetScheduleType(ScheduledFeature::ScheduleType::kCustom);
  EXPECT_TRUE(GetEnabled());

  // Simulate reaching the end time by triggering the timer's user task. Make
  // sure that the feature ended.
  //
  //      15:00                      20:00
  // <----- + ------------------------ + ----->
  //        |                          |
  //      start                    end & now
  //
  // Now is 8:00 PM.
  test_clock_.SetNow(MakeTimeOfDay(8, AmPm::kPM).ToTimeToday());
  feature()->timer()->FireNow();
  EXPECT_FALSE(GetEnabled());
  // The timer should still be running, but now scheduling the start at 3:00 PM
  // tomorrow which is 19 hours from "now" (8:00 PM).
  EXPECT_TRUE(feature()->timer()->IsRunning());
  EXPECT_EQ(base::Hours(19), feature()->timer()->GetCurrentDelay());
}

// Tests that user toggles from the system menu or system settings override any
// status set by an automatic schedule.
TEST_F(ScheduledFeatureTest, ExplicitUserTogglesWhileScheduleIsActive) {
  // Start with the below custom schedule, where the feature is off.
  //
  //      15:00               20:00          23:00
  // <----- + ----------------- + ------------ + ---->
  //        |                   |              |
  //      start                end            now
  //
  test_clock_.SetNow(MakeTimeOfDay(11, AmPm::kPM).ToTimeToday());
  feature()->SetCustomStartTime(MakeTimeOfDay(3, AmPm::kPM));
  feature()->SetCustomEndTime(MakeTimeOfDay(8, AmPm::kPM));
  feature()->SetScheduleType(ScheduledFeature::ScheduleType::kCustom);
  EXPECT_FALSE(GetEnabled());

  // What happens if the user manually turns the feature on while the schedule
  // type says it should be off?
  // User toggles either from the system menu or the System Settings toggle
  // button must override any automatic schedule.
  SetFeatureEnabled(true);
  EXPECT_TRUE(GetEnabled());
  // The timer should still be running, but the feature should automatically
  // turn off at 8:00 PM tomorrow, which is 21 hours from now (11:00 PM).
  EXPECT_TRUE(feature()->timer()->IsRunning());
  EXPECT_EQ(base::Hours(21), feature()->timer()->GetCurrentDelay());

  // Manually turning it back off should also be respected, and this time the
  // start is scheduled at 3:00 PM tomorrow after 19 hours from "now" (8:00
  // PM).
  SetFeatureEnabled(false);
  EXPECT_FALSE(GetEnabled());
  EXPECT_TRUE(feature()->timer()->IsRunning());
  EXPECT_EQ(base::Hours(16), feature()->timer()->GetCurrentDelay());
}

// Tests that changing the custom start and end times, in such a way that
// shouldn't change the current status, only updates the timer but doesn't
// change the status.
TEST_F(ScheduledFeatureTest, ChangingStartTimesThatDontChangeTheStatus) {
  //       16:00        18:00         22:00
  // <----- + ----------- + ----------- + ----->
  //        |             |             |
  //       now          start          end
  //
  test_clock_.SetNow(MakeTimeOfDay(4, AmPm::kPM).ToTimeToday());  // 4:00 PM.
  SetFeatureEnabled(false);
  feature()->SetScheduleType(ScheduledFeature::ScheduleType::kNone);
  feature()->SetCustomStartTime(MakeTimeOfDay(6, AmPm::kPM));  // 6:00 PM.
  feature()->SetCustomEndTime(MakeTimeOfDay(10, AmPm::kPM));   // 10:00 PM.

  // Since now is outside the feature interval, changing the schedule type
  // to kCustom, shouldn't affect the status. Validate the timer is running
  // with a 2-hour delay.
  feature()->SetScheduleType(ScheduledFeature::ScheduleType::kCustom);
  EXPECT_FALSE(GetEnabled());
  EXPECT_TRUE(feature()->timer()->IsRunning());
  EXPECT_EQ(base::Hours(2), feature()->timer()->GetCurrentDelay());

  // Change the start time in such a way that doesn't change the status, but
  // despite that, confirm that schedule has been updated.
  feature()->SetCustomStartTime(MakeTimeOfDay(7, AmPm::kPM));  // 7:00 PM.
  EXPECT_FALSE(GetEnabled());
  EXPECT_TRUE(feature()->timer()->IsRunning());
  EXPECT_EQ(base::Hours(3), feature()->timer()->GetCurrentDelay());

  // Changing the end time in a similar fashion to the above and expect no
  // change.
  feature()->SetCustomEndTime(MakeTimeOfDay(11, AmPm::kPM));  // 11:00 PM.
  EXPECT_FALSE(GetEnabled());
  EXPECT_TRUE(feature()->timer()->IsRunning());
  EXPECT_EQ(base::Hours(3), feature()->timer()->GetCurrentDelay());
}

// Tests that the feature should turn on at sunset time and turn off at sunrise
// time.
TEST_F(ScheduledFeatureTest, SunsetSunrise) {
  EXPECT_FALSE(GetEnabled());

  // Set time now to 10:00 AM.
  base::Time current_time = MakeTimeOfDay(10, AmPm::kAM).ToTimeToday();
  test_clock_.SetNow(current_time);
  EXPECT_FALSE(feature()->timer()->IsRunning());
  feature()->SetScheduleType(ScheduledFeature::ScheduleType::kSunsetToSunrise);
  EXPECT_FALSE(GetEnabled());
  EXPECT_TRUE(feature()->timer()->IsRunning());
  EXPECT_EQ(geolocation_controller_->GetSunsetTime() - current_time,
            feature()->timer()->GetCurrentDelay());

  // Firing a timer should to advance the time to sunset and automatically turn
  // on the feature.
  current_time = geolocation_controller_->GetSunsetTime();
  test_clock_.SetNow(current_time);
  feature()->timer()->FireNow();
  EXPECT_TRUE(feature()->timer()->IsRunning());
  EXPECT_TRUE(GetEnabled());
  EXPECT_EQ(
      geolocation_controller_->GetSunriseTime() + base::Days(1) - current_time,
      feature()->timer()->GetCurrentDelay());

  // Firing a timer should advance the time to sunrise and automatically turn
  // off the feature.
  current_time = geolocation_controller_->GetSunriseTime();
  test_clock_.SetNow(current_time);
  feature()->timer()->FireNow();
  EXPECT_FALSE(GetEnabled());
  EXPECT_TRUE(feature()->timer()->IsRunning());
  EXPECT_EQ(geolocation_controller_->GetSunsetTime() - current_time,
            feature()->timer()->GetCurrentDelay());
}

// Tests that scheduled start time and end time of sunset-to-sunrise feature
// are updated correctly if the geoposition changes.
TEST_F(ScheduledFeatureTest, SunsetSunriseGeoposition) {
  constexpr double kFakePosition1_Latitude = 23.5;
  constexpr double kFakePosition1_Longitude = 55.88;
  constexpr double kFakePosition2_Latitude = 23.5;
  constexpr double kFakePosition2_Longitude = 10.9;
  // Position 1 sunset and sunrise times.
  //
  //    sunset-4
  // <----- + --------- + ---------------- + ------->
  //        |           |                  |
  //       now        sunset            sunrise
  //

  // Prepare a valid geoposition.
  const Geoposition position = CreateGeoposition(
      kFakePosition1_Latitude, kFakePosition1_Longitude, test_clock_.Now());

  GeolocationControllerObserver observer1;
  geolocation_controller_->AddObserver(&observer1);
  EXPECT_TRUE(mock_timer_ptr_->IsRunning());
  geolocation_controller_->set_position_to_send(position);
  EXPECT_FALSE(observer1.possible_change_in_timezone());

  // Fire timer to fetch position update.
  mock_timer_ptr_->Fire();
  EXPECT_TRUE(observer1.possible_change_in_timezone());
  const base::Time sunset_time1 = geolocation_controller_->GetSunsetTime();
  const base::Time sunrise_time1 = geolocation_controller_->GetSunriseTime();
  // Our assumption is that GeolocationController gives us sunrise time
  // earlier in the same day before sunset.
  ASSERT_GT(sunset_time1, sunrise_time1);
  ASSERT_LT(sunset_time1 - base::Days(1), sunrise_time1);

  // Set time now to be 4 hours before sunset.
  test_clock_.SetNow(sunset_time1 - base::Hours(4));

  // Expect that timer is running and the start is scheduled after 4 hours.
  EXPECT_FALSE(feature()->GetEnabled());
  feature()->SetScheduleType(ScheduledFeature::ScheduleType::kSunsetToSunrise);
  EXPECT_FALSE(feature()->GetEnabled());
  EXPECT_TRUE(feature()->timer()->IsRunning());
  EXPECT_EQ(base::Hours(4), feature()->timer()->GetCurrentDelay());

  // Simulate reaching sunset.
  test_clock_.SetNow(sunset_time1);  // Now is sunset time of the position1.
  feature()->timer()->FireNow();
  EXPECT_TRUE(feature()->GetEnabled());
  // Timer is running scheduling the end at sunrise of the second day.
  EXPECT_TRUE(feature()->timer()->IsRunning());
  EXPECT_EQ(sunrise_time1 + base::Days(1) - test_clock_.Now(),
            feature()->timer()->GetCurrentDelay());

  // Simulate reaching sunrise.
  test_clock_.SetNow(sunrise_time1);  // Now is sunrise time of the position1

  // Now simulate user changing position.
  // Position 2 sunset and sunrise times.
  //
  // <----- + --------- + ---------------- + ------->
  //        |           |                  |
  //      sunset2      now (sunrise1)     sunrise2
  //

  const Geoposition position2 = CreateGeoposition(
      kFakePosition2_Latitude, kFakePosition2_Longitude, test_clock_.Now());
  geolocation_controller_->set_position_to_send(position2);
  mock_timer_ptr_->Fire();
  EXPECT_TRUE(observer1.possible_change_in_timezone());
  const base::Time sunset_time2 = geolocation_controller_->GetSunsetTime();
  const base::Time sunrise_time2 = geolocation_controller_->GetSunriseTime();
  // We choose the second location such that the new sunrise time is later
  // in the day compared to the old sunrise time, which is also the current
  // time.
  ASSERT_GT(test_clock_.Now(), sunset_time2);
  ASSERT_LT(test_clock_.Now(), sunset_time2 + base::Days(1));
  ASSERT_GT(test_clock_.Now(), sunrise_time2);
  ASSERT_LT(test_clock_.Now(), sunrise_time2 + base::Days(1));

  // Expect that the scheduled end delay has been updated to sunrise of the
  // same (second) day in location 2, and the status hasn't changed.
  EXPECT_TRUE(feature()->GetEnabled());
  EXPECT_TRUE(feature()->timer()->IsRunning());
  EXPECT_EQ(sunrise_time2 + base::Days(1) - test_clock_.Now(),
            feature()->timer()->GetCurrentDelay());

  // Simulate reaching sunrise.
  test_clock_.SetNow(sunrise_time2 +
                     base::Days(1));  // Now is sunrise time of the position2.
  feature()->timer()->FireNow();
  EXPECT_FALSE(feature()->GetEnabled());
  // Timer is running scheduling the start at the sunset of the next day.
  EXPECT_TRUE(feature()->timer()->IsRunning());
  EXPECT_EQ(sunset_time2 + base::Days(1) - test_clock_.Now(),
            feature()->timer()->GetCurrentDelay());
}

// Tests that on device resume from sleep, the feature status is updated
// correctly if the time has changed meanwhile.
TEST_F(ScheduledFeatureTest, CustomScheduleOnResume) {
  // Now is 4:00 PM.
  test_clock_.SetNow(MakeTimeOfDay(4, AmPm::kPM).ToTimeToday());
  feature()->SetEnabled(false);
  // Start time is at 6:00 PM and end time is at 10:00 PM. The feature should be
  // off.
  //      16:00         18:00         22:00
  // <----- + ----------- + ----------- + ----->
  //        |             |             |
  //       now          start          end
  //
  feature()->SetCustomStartTime(MakeTimeOfDay(6, AmPm::kPM));
  feature()->SetCustomEndTime(MakeTimeOfDay(10, AmPm::kPM));
  feature()->SetScheduleType(ScheduledFeature::ScheduleType::kCustom);

  EXPECT_FALSE(feature()->GetEnabled());
  EXPECT_TRUE(feature()->timer()->IsRunning());
  // The feature should be enabled in 2 hours.
  EXPECT_EQ(base::Hours(2), feature()->timer()->GetCurrentDelay());

  // Now simulate that the device was suspended for 3 hours, and the time now
  // is 7:00 PM when the devices was resumed. Expect that the feature turns on.
  test_clock_.SetNow(MakeTimeOfDay(7, AmPm::kPM).ToTimeToday());
  feature()->SuspendDone(base::TimeDelta::Max());

  EXPECT_TRUE(feature()->GetEnabled());
  EXPECT_TRUE(feature()->timer()->IsRunning());
  // The feature should be disabled in 3 hours.
  EXPECT_EQ(base::Hours(3), feature()->timer()->GetCurrentDelay());
}

// The following tests ensure that the feature schedule is correctly
// refreshed when the start and end times are inverted (i.e. the "start time" as
// a time of day today is in the future with respect to the "end time" also as a
// time of day today).
//
// Case 1: "Now" is less than both "end" and "start".
TEST_F(ScheduledFeatureTest, CustomScheduleInvertedStartAndEndTimesCase1) {
  // Now is 4:00 AM.
  test_clock_.SetNow(MakeTimeOfDay(4, AmPm::kAM).ToTimeToday());
  SetFeatureEnabled(false);
  // Start time is at 9:00 PM and end time is at 6:00 AM. "Now" is less than
  // both. The feature should be on.
  //       4:00          6:00         21:00
  // <----- + ----------- + ----------- + ----->
  //        |             |             |
  //       now           end          start
  //
  feature()->SetCustomStartTime(MakeTimeOfDay(9, AmPm::kPM));
  feature()->SetCustomEndTime(MakeTimeOfDay(6, AmPm::kAM));
  feature()->SetScheduleType(ScheduledFeature::ScheduleType::kCustom);

  EXPECT_TRUE(GetEnabled());
  EXPECT_TRUE(feature()->timer()->IsRunning());
  // The feature should end in two hours.
  EXPECT_EQ(base::Hours(2), feature()->timer()->GetCurrentDelay());
}

// Case 2: "Now" is between "end" and "start".
TEST_F(ScheduledFeatureTest, CustomScheduleInvertedStartAndEndTimesCase2) {
  // Now is 6:00 AM.
  test_clock_.SetNow(MakeTimeOfDay(6, AmPm::kAM).ToTimeToday());
  SetFeatureEnabled(false);
  // Start time is at 9:00 PM and end time is at 4:00 AM. "Now" is between both.
  // The feature should be off.
  //       4:00          6:00         21:00
  // <----- + ----------- + ----------- + ----->
  //        |             |             |
  //       end           now          start
  //
  feature()->SetCustomStartTime(MakeTimeOfDay(9, AmPm::kPM));
  feature()->SetCustomEndTime(MakeTimeOfDay(4, AmPm::kAM));
  feature()->SetScheduleType(ScheduledFeature::ScheduleType::kCustom);

  EXPECT_FALSE(GetEnabled());
  EXPECT_TRUE(feature()->timer()->IsRunning());
  // The feature should start in 15 hours.
  EXPECT_EQ(base::Hours(15), feature()->timer()->GetCurrentDelay());
}

// Case 3: "Now" is greater than both "start" and "end".
TEST_F(ScheduledFeatureTest, CustomScheduleInvertedStartAndEndTimesCase3) {
  // Now is 11:00 PM.
  test_clock_.SetNow(MakeTimeOfDay(11, AmPm::kPM).ToTimeToday());
  SetFeatureEnabled(false);
  // Start time is at 9:00 PM and end time is at 4:00 AM. "Now" is greater than
  // both. the feature should be on.
  //       4:00         21:00         23:00
  // <----- + ----------- + ----------- + ----->
  //        |             |             |
  //       end          start          now
  //
  feature()->SetCustomStartTime(MakeTimeOfDay(9, AmPm::kPM));
  feature()->SetCustomEndTime(MakeTimeOfDay(4, AmPm::kAM));
  feature()->SetScheduleType(ScheduledFeature::ScheduleType::kCustom);

  EXPECT_TRUE(GetEnabled());
  EXPECT_TRUE(feature()->timer()->IsRunning());
  // The feature should end in 5 hours.
  EXPECT_EQ(base::Hours(5), feature()->timer()->GetCurrentDelay());
}

// Tests that manual changes to the feature status while a schedule is being
// used will be remembered and reapplied across user switches.
TEST_F(ScheduledFeatureTest, MultiUserManualStatusToggleWithSchedules) {
  // Setup user 1 to use a custom schedule from 3pm till 8pm, and user 2 to use
  // a sunset-to-sunrise schedule from 6pm till 6am.
  //
  //
  //          |<--- User 1 NL on --->|
  //          |                      |
  // <--------+-------------+--------+----------------------------+----------->
  //         3pm           6pm      8pm                          6am
  //                        |                                     |
  //                        |<----------- User 2 NL on ---------->|
  //
  // Test cases at:
  //
  // <---+---------+------------+------------+----------------------------+--->
  //    2pm       4pm         7pm           10pm                         9am
  //

  test_clock_.SetNow(MakeTimeOfDay(2, kPM).ToTimeToday());
  feature()->SetCustomStartTime(MakeTimeOfDay(3, kPM));
  feature()->SetCustomEndTime(MakeTimeOfDay(8, kPM));
  feature()->SetScheduleType(ScheduledFeature::ScheduleType::kCustom);
  SwitchActiveUser(kUser2Email);
  feature()->SetScheduleType(ScheduledFeature::ScheduleType::kSunsetToSunrise);
  SwitchActiveUser(kUser1Email);

  struct {
    base::Time fake_now;
    bool user_1_expected_status;
    bool user_2_expected_status;
  } kTestCases[] = {
      {MakeTimeOfDay(2, kPM).ToTimeToday(), false, false},
      {MakeTimeOfDay(4, kPM).ToTimeToday(), true, false},
      {MakeTimeOfDay(7, kPM).ToTimeToday(), true, true},
      {MakeTimeOfDay(10, kPM).ToTimeToday(), false, true},
      {MakeTimeOfDay(9, kAM).ToTimeToday() +
           base::Days(1),  // 9:00 AM tomorrow.
       false, false},
  };

  bool user_1_previous_status = false;
  for (const auto& test_case : kTestCases) {
    // Each test case begins when user_1 is active.
    const bool user_1_toggled_status = !test_case.user_1_expected_status;
    const bool user_2_toggled_status = !test_case.user_2_expected_status;

    // Apply the test's case fake time, and fire the timer if there's a change
    // expected in the feature's status.
    test_clock_.SetNow(test_case.fake_now);
    if (user_1_previous_status != test_case.user_1_expected_status)
      feature()->timer()->FireNow();
    user_1_previous_status = test_case.user_1_expected_status;

    // The untoggled states for both users should match the expected ones
    // according to their schedules.
    EXPECT_EQ(test_case.user_1_expected_status, feature()->GetEnabled());
    SwitchActiveUser(kUser2Email);
    EXPECT_EQ(test_case.user_2_expected_status, feature()->GetEnabled());

    // Manually toggle the feature for user_2 and expect that it will be
    // remembered when we switch to user_1 and back.
    feature()->SetEnabled(user_2_toggled_status);
    EXPECT_EQ(user_2_toggled_status, feature()->GetEnabled());
    SwitchActiveUser(kUser1Email);
    EXPECT_EQ(test_case.user_1_expected_status, feature()->GetEnabled());
    SwitchActiveUser(kUser2Email);
    EXPECT_EQ(user_2_toggled_status, feature()->GetEnabled());

    // Toggle it for user_1 as well, and expect it will be remembered and won't
    // affect the already toggled state for user_2.
    SwitchActiveUser(kUser1Email);
    EXPECT_EQ(test_case.user_1_expected_status, feature()->GetEnabled());
    feature()->SetEnabled(user_1_toggled_status);
    EXPECT_EQ(user_1_toggled_status, feature()->GetEnabled());
    SwitchActiveUser(kUser2Email);
    EXPECT_EQ(user_2_toggled_status, feature()->GetEnabled());

    // Toggle both users back to their original states in preparation for the
    // next test case.
    feature()->SetEnabled(test_case.user_2_expected_status);
    EXPECT_EQ(test_case.user_2_expected_status, feature()->GetEnabled());
    SwitchActiveUser(kUser1Email);
    EXPECT_EQ(user_1_toggled_status, feature()->GetEnabled());
    feature()->SetEnabled(test_case.user_1_expected_status);
    EXPECT_EQ(test_case.user_1_expected_status, feature()->GetEnabled());
  }
}

TEST_F(ScheduledFeatureTest,
       ManualStatusToggleCanPersistAfterResumeFromSuspend) {
  test_clock_.SetNow(MakeTimeOfDay(11, kAM).ToTimeToday());

  feature()->SetCustomStartTime(MakeTimeOfDay(3, kPM));
  feature()->SetCustomEndTime(MakeTimeOfDay(8, kPM));
  feature()->SetScheduleType(ScheduledFeature::ScheduleType::kCustom);
  EXPECT_FALSE(feature()->GetEnabled());

  // Toggle the status manually and expect that the feature is scheduled to
  // turn back off at 8:00 PM.
  feature()->SetEnabled(true);
  EXPECT_TRUE(feature()->GetEnabled());
  EXPECT_TRUE(feature()->timer()->IsRunning());
  EXPECT_EQ(base::Hours(9), feature()->timer()->GetCurrentDelay());

  // Simulate suspend and then resume at 2:00 PM (which is outside the user's
  // custom schedule). However, the manual toggle to on should be kept.
  test_clock_.SetNow(MakeTimeOfDay(2, kPM).ToTimeToday());
  feature()->SuspendDone(base::TimeDelta{});
  EXPECT_TRUE(feature()->GetEnabled());

  // Suspend again and resume at 5:00 PM (which is within the user's custom
  // schedule). The schedule should be applied normally.
  test_clock_.SetNow(MakeTimeOfDay(5, kPM).ToTimeToday());
  feature()->SuspendDone(base::TimeDelta{});
  EXPECT_TRUE(feature()->GetEnabled());

  // Suspend and resume at 9:00 PM and expect the feature to be off.
  test_clock_.SetNow(MakeTimeOfDay(9, kPM).ToTimeToday());
  feature()->SuspendDone(base::TimeDelta{});
  EXPECT_FALSE(feature()->GetEnabled());
}

}  // namespace

}  // namespace ash