// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/throttle_service.h"

#include <utility>

#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

class TestObserver : public ThrottleService::ServiceObserver {
 public:
  TestObserver() = default;
  ~TestObserver() override = default;

  // ThrottleService::Observer:
  void OnThrottle(ThrottleObserver::PriorityLevel level) override {
    last_level_ = level;
    ++update_count_;
  }

  int GetUpdateCountAndReset() {
    const int update_count = update_count_;
    update_count_ = 0;
    return update_count;
  }

  ThrottleObserver::PriorityLevel last_level() const { return last_level_; }

 private:
  int update_count_ = 0;
  ThrottleObserver::PriorityLevel last_level_ =
      ThrottleObserver::PriorityLevel::UNKNOWN;

  TestObserver(TestObserver const&) = delete;
  TestObserver& operator=(TestObserver const&) = delete;
};

}  // namespace

class TestThrottleService : public ThrottleService {
 public:
  using ThrottleService::ThrottleService;

  size_t throttle_instance_count() const { return throttle_instance_count_; }

  size_t uma_count() { return record_uma_counter_; }

  ThrottleObserver::PriorityLevel last_throttle_level() const {
    return last_throttle_level_;
  }

  const std::string& last_recorded_observer_name() {
    return last_recorded_observer_name_;
  }

 private:
  void ThrottleInstance(ThrottleObserver::PriorityLevel level) override {
    ++throttle_instance_count_;
    last_throttle_level_ = level;
  }

  void RecordCpuRestrictionDisabledUMA(const std::string& observer_name,
                                       base::TimeDelta delta) override {
    ++record_uma_counter_;
    last_recorded_observer_name_ = observer_name;
  }

  size_t throttle_instance_count_{0};
  size_t record_uma_counter_{0};
  std::string last_recorded_observer_name_;
  ThrottleObserver::PriorityLevel last_throttle_level_{
      ThrottleObserver::PriorityLevel::UNKNOWN};
};

class ThrottleServiceTest : public testing::Test {
 public:
  ThrottleServiceTest() : service_(&profile_) {
    std::vector<std::unique_ptr<ThrottleObserver>> observers;
    observers.push_back(std::make_unique<TestCriticalObserver>(this));
    observers.push_back(std::make_unique<TestLowObserver>(this));
    service_.SetObserversForTesting(std::move(observers));
  }

  ThrottleServiceTest(const ThrottleServiceTest&) = delete;
  ThrottleServiceTest& operator=(const ThrottleServiceTest&) = delete;

  void set_critical_observer(ThrottleObserver* observer) {
    critical_observer_ = observer;
  }

  void set_low_observer(ThrottleObserver* observer) {
    low_observer_ = observer;
  }

 protected:
  TestThrottleService* service() { return &service_; }

  ThrottleObserver* critical_observer() { return critical_observer_; }

  ThrottleObserver* low_observer() { return low_observer_; }

 private:
  class TestCriticalObserver : public ThrottleObserver {
   public:
    explicit TestCriticalObserver(ThrottleServiceTest* test)
        : ThrottleObserver(ThrottleObserver::PriorityLevel::CRITICAL,
                           "CriticalObserver"),
          test_(test) {
      test_->set_critical_observer(this);
    }
    ~TestCriticalObserver() override { test_->set_critical_observer(nullptr); }

   private:
    ThrottleServiceTest* test_;
  };
  class TestLowObserver : public ThrottleObserver {
   public:
    explicit TestLowObserver(ThrottleServiceTest* test)
        : ThrottleObserver(ThrottleObserver::PriorityLevel::LOW, "LowObserver"),
          test_(test) {
      test_->set_low_observer(this);
    }
    ~TestLowObserver() override { test_->set_low_observer(nullptr); }

   private:
    ThrottleServiceTest* test_;
  };

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  TestThrottleService service_;
  ThrottleObserver* critical_observer_{nullptr};
  ThrottleObserver* low_observer_{nullptr};
};

TEST_F(ThrottleServiceTest, TestConstructDestruct) {}

// Tests that the ThrottleService calls ThrottleInstance with the correct level
// when there is a change in observers, but skips the call if new level is same
// as before.
TEST_F(ThrottleServiceTest, TestOnObserverStateChanged) {
  EXPECT_EQ(0U, service()->throttle_instance_count());

  service()->NotifyObserverStateChangedForTesting();
  EXPECT_EQ(1U, service()->throttle_instance_count());
  EXPECT_EQ(ThrottleObserver::PriorityLevel::LOW,
            service()->last_throttle_level());

  // ThrottleService level is already LOW, expect no change.
  low_observer()->SetActive(true);
  EXPECT_EQ(1U, service()->throttle_instance_count());

  critical_observer()->SetActive(true);
  EXPECT_EQ(2U, service()->throttle_instance_count());
  EXPECT_EQ(ThrottleObserver::PriorityLevel::CRITICAL,
            service()->last_throttle_level());

  critical_observer()->SetActive(false);
  EXPECT_EQ(3U, service()->throttle_instance_count());
  EXPECT_EQ(ThrottleObserver::PriorityLevel::LOW,
            service()->last_throttle_level());
}

// Tests that ArcInstanceThrottle records the duration that the effective
// observer is active.
TEST_F(ThrottleServiceTest, RecordCpuRestrictionDisabledUMA) {
  EXPECT_EQ(0U, service()->uma_count());

  // The effective observer transitions from null to critical_observer; no UMA
  // is recorded yet.
  critical_observer()->SetActive(true);
  EXPECT_EQ(0U, service()->uma_count());
  low_observer()->SetActive(true);
  EXPECT_EQ(0U, service()->uma_count());

  // The effective observer transitions from critical_observer to low_observer;
  // UMA should be recorded for critical_observer.
  critical_observer()->SetActive(false);
  EXPECT_EQ(1U, service()->uma_count());
  EXPECT_EQ(critical_observer()->name(),
            service()->last_recorded_observer_name());

  // Effective observer transitions from low_observer to critical_observer; UMA
  // should be recorded for low_observer.
  critical_observer()->SetActive(true);
  EXPECT_EQ(2U, service()->uma_count());
  EXPECT_EQ(low_observer()->name(), service()->last_recorded_observer_name());

  // Effective observer transitions from critical_observer to null; UMA should
  // be recorded for critical_observer.
  low_observer()->SetActive(false);
  critical_observer()->SetActive(false);
  EXPECT_EQ(3U, service()->uma_count());
  EXPECT_EQ(critical_observer()->name(),
            service()->last_recorded_observer_name());
}

// Tests that verifies enforcement mode.
TEST_F(ThrottleServiceTest, TestEnforced) {
  low_observer()->SetActive(true);
  EXPECT_EQ(ThrottleObserver::PriorityLevel::LOW, service()->level());
  service()->SetEnforced(ThrottleObserver::PriorityLevel::NORMAL);
  EXPECT_EQ(ThrottleObserver::PriorityLevel::NORMAL, service()->level());
  service()->SetEnforced(ThrottleObserver::PriorityLevel::UNKNOWN);
  EXPECT_EQ(ThrottleObserver::PriorityLevel::LOW, service()->level());

  low_observer()->SetActive(false);
  critical_observer()->SetActive(true);
  EXPECT_EQ(ThrottleObserver::PriorityLevel::CRITICAL,
            service()->last_throttle_level());
  service()->SetEnforced(ThrottleObserver::PriorityLevel::LOW);
  EXPECT_EQ(ThrottleObserver::PriorityLevel::LOW, service()->level());
  service()->SetEnforced(ThrottleObserver::PriorityLevel::UNKNOWN);
  EXPECT_EQ(ThrottleObserver::PriorityLevel::CRITICAL, service()->level());
}

// Tests that verifies observer notifications.
TEST_F(ThrottleServiceTest, TestObservers) {
  TestObserver test_observer;
  service()->AddServiceObserver(&test_observer);
  EXPECT_EQ(0, test_observer.GetUpdateCountAndReset());
  low_observer()->SetActive(true);
  EXPECT_EQ(ThrottleObserver::PriorityLevel::LOW, test_observer.last_level());
  EXPECT_EQ(ThrottleObserver::PriorityLevel::LOW, service()->level());
  EXPECT_EQ(1, test_observer.GetUpdateCountAndReset());
  low_observer()->SetActive(false);
  critical_observer()->SetActive(true);
  EXPECT_EQ(ThrottleObserver::PriorityLevel::CRITICAL,
            test_observer.last_level());
  EXPECT_EQ(ThrottleObserver::PriorityLevel::CRITICAL, service()->level());
  EXPECT_EQ(1, test_observer.GetUpdateCountAndReset());
  service()->RemoveServiceObserver(&test_observer);
  critical_observer()->SetActive(false);
  low_observer()->SetActive(true);
  EXPECT_EQ(ThrottleObserver::PriorityLevel::CRITICAL,
            test_observer.last_level());
  EXPECT_EQ(ThrottleObserver::PriorityLevel::LOW, service()->level());
  EXPECT_EQ(0, test_observer.GetUpdateCountAndReset());
}

}  // namespace ash
