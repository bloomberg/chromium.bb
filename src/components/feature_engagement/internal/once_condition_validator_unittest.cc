// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/once_condition_validator.h"

#include <string>

#include "base/feature_list.h"
#include "components/feature_engagement/internal/editable_configuration.h"
#include "components/feature_engagement/internal/event_model.h"
#include "components/feature_engagement/internal/never_availability_model.h"
#include "components/feature_engagement/internal/noop_display_lock_controller.h"
#include "components/feature_engagement/internal/proto/feature_event.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feature_engagement {

namespace {

const base::Feature kOnceTestFeatureFoo{"test_foo",
                                        base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kOnceTestFeatureBar{"test_bar",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

FeatureConfig kValidFeatureConfig;
FeatureConfig kInvalidFeatureConfig;

// A EventModel that is easily configurable at runtime.
class OnceTestEventModel : public EventModel {
 public:
  OnceTestEventModel() : ready_(false) { kValidFeatureConfig.valid = true; }

  void Initialize(const OnModelInitializationFinished& callback,
                  uint32_t current_day) override {}

  bool IsReady() const override { return ready_; }

  void SetIsReady(bool ready) { ready_ = ready; }

  const Event* GetEvent(const std::string& event_name) const override {
    return nullptr;
  }

  uint32_t GetEventCount(const std::string& event_name,
                         uint32_t current_day,
                         uint32_t window_size) const override {
    return 0;
  }

  void IncrementEvent(const std::string& event_name, uint32_t day) override {}

 private:
  bool ready_;
};

class OnceConditionValidatorTest : public ::testing::Test {
 public:
  OnceConditionValidatorTest() {
    // By default, event model should be ready.
    event_model_.SetIsReady(true);
  }

 protected:
  EditableConfiguration configuration_;
  OnceTestEventModel event_model_;
  NeverAvailabilityModel availability_model_;
  NoopDisplayLockController display_lock_controller_;
  OnceConditionValidator validator_;

 private:
  DISALLOW_COPY_AND_ASSIGN(OnceConditionValidatorTest);
};

}  // namespace

TEST_F(OnceConditionValidatorTest, EnabledFeatureShouldTriggerOnce) {
  // Only the first call to MeetsConditions() should lead to enlightenment.
  EXPECT_TRUE(validator_
                  .MeetsConditions(kOnceTestFeatureFoo, kValidFeatureConfig,
                                   event_model_, availability_model_,
                                   display_lock_controller_, 0u)
                  .NoErrors());
  validator_.NotifyIsShowing(kOnceTestFeatureFoo, FeatureConfig(), {""});
  ConditionValidator::Result result = validator_.MeetsConditions(
      kOnceTestFeatureFoo, kValidFeatureConfig, event_model_,
      availability_model_, display_lock_controller_, 0u);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.session_rate_ok);
  EXPECT_FALSE(result.trigger_ok);
}

TEST_F(OnceConditionValidatorTest,
       BothEnabledAndDisabledFeaturesShouldTrigger) {
  // Only the kOnceTestFeatureFoo feature should lead to enlightenment, since
  // kOnceTestFeatureBar is disabled. Ordering disabled feature first to ensure
  // this captures a different behavior than the
  // OnlyOneFeatureShouldTriggerPerSession test below.
  EXPECT_TRUE(validator_
                  .MeetsConditions(kOnceTestFeatureBar, kValidFeatureConfig,
                                   event_model_, availability_model_,
                                   display_lock_controller_, 0u)
                  .NoErrors());
  EXPECT_TRUE(validator_
                  .MeetsConditions(kOnceTestFeatureFoo, kValidFeatureConfig,
                                   event_model_, availability_model_,
                                   display_lock_controller_, 0u)
                  .NoErrors());
}

TEST_F(OnceConditionValidatorTest, StillTriggerWhenAllFeaturesDisabled) {
  // No features should get to show enlightenment.
  EXPECT_TRUE(validator_
                  .MeetsConditions(kOnceTestFeatureFoo, kValidFeatureConfig,
                                   event_model_, availability_model_,
                                   display_lock_controller_, 0u)
                  .NoErrors());
  EXPECT_TRUE(validator_
                  .MeetsConditions(kOnceTestFeatureBar, kValidFeatureConfig,
                                   event_model_, availability_model_,
                                   display_lock_controller_, 0u)
                  .NoErrors());
}

TEST_F(OnceConditionValidatorTest, OnlyTriggerWhenModelIsReady) {
  event_model_.SetIsReady(false);
  ConditionValidator::Result result = validator_.MeetsConditions(
      kOnceTestFeatureFoo, kValidFeatureConfig, event_model_,
      availability_model_, display_lock_controller_, 0u);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.event_model_ready_ok);

  event_model_.SetIsReady(true);
  EXPECT_TRUE(validator_
                  .MeetsConditions(kOnceTestFeatureFoo, kValidFeatureConfig,
                                   event_model_, availability_model_,
                                   display_lock_controller_, 0u)
                  .NoErrors());
}

TEST_F(OnceConditionValidatorTest, OnlyTriggerIfNothingElseIsShowing) {
  validator_.NotifyIsShowing(kOnceTestFeatureBar, FeatureConfig(), {""});
  ConditionValidator::Result result = validator_.MeetsConditions(
      kOnceTestFeatureFoo, kValidFeatureConfig, event_model_,
      availability_model_, display_lock_controller_, 0u);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.currently_showing_ok);

  validator_.NotifyDismissed(kOnceTestFeatureBar);
  EXPECT_TRUE(validator_
                  .MeetsConditions(kOnceTestFeatureFoo, kValidFeatureConfig,
                                   event_model_, availability_model_,
                                   display_lock_controller_, 0u)
                  .NoErrors());
}

TEST_F(OnceConditionValidatorTest, DoNotTriggerForInvalidConfig) {
  ConditionValidator::Result result = validator_.MeetsConditions(
      kOnceTestFeatureFoo, kInvalidFeatureConfig, event_model_,
      availability_model_, display_lock_controller_, 0u);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.config_ok);

  EXPECT_TRUE(validator_
                  .MeetsConditions(kOnceTestFeatureFoo, kValidFeatureConfig,
                                   event_model_, availability_model_,
                                   display_lock_controller_, 0u)
                  .NoErrors());
}

}  // namespace feature_engagement
