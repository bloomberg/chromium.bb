// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/time_limit_consistency_test/consistency_golden_converter.h"

#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chromeos/child_accounts/time_limit_test_utils.h"

namespace chromeos {

namespace utils = time_limit_test_utils;

namespace time_limit_consistency {
namespace {

// Converts a ActivePolicies object from the time limit processor to a
// ConsistencyGoldenPolicy used by the goldens.
ConsistencyGoldenPolicy ConvertProcessorPolicyToGoldenPolicy(
    usage_time_limit::ActivePolicies processor_policy) {
  switch (processor_policy) {
    case usage_time_limit::ActivePolicies::kOverride:
      return OVERRIDE;
    case usage_time_limit::ActivePolicies::kFixedLimit:
      return FIXED_LIMIT;
    case usage_time_limit::ActivePolicies::kUsageLimit:
      return USAGE_LIMIT;
    case usage_time_limit::ActivePolicies::kNoActivePolicy:
      return NO_ACTIVE_POLICY;
  }

  NOTREACHED();
  return UNSPECIFIED_POLICY;
}

// Converts the representation of a day of week used by the goldens to the one
// used by the time limit processor.
const char* ConvertGoldenDayToProcessorDay(ConsistencyGoldenEffectiveDay day) {
  switch (day) {
    case MONDAY:
      return utils::kMonday;
    case TUESDAY:
      return utils::kTuesday;
    case WEDNESDAY:
      return utils::kWednesday;
    case THURSDAY:
      return utils::kThursday;
    case FRIDAY:
      return utils::kFriday;
    case SATURDAY:
      return utils::kSaturday;
    case SUNDAY:
      return utils::kSunday;
    default:
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace

std::unique_ptr<base::DictionaryValue> ConvertGoldenInputToProcessorInput(
    ConsistencyGoldenInput input) {
  // Random date representing the last time the policies were updated,
  // since the tests won't take this into account for now.
  base::Time last_updated = utils::TimeFromString("1 Jan 2018 10:00 GMT+0300");

  std::unique_ptr<base::DictionaryValue> policy =
      utils::CreateTimeLimitPolicy(base::TimeDelta::FromHours(6));

  /* Begin Window Limits data */

  if (input.window_limits_size() > 0) {
    for (ConsistencyGoldenWindowLimitEntry window_limit :
         input.window_limits()) {
      utils::AddTimeWindowLimit(
          policy.get(),
          ConvertGoldenDayToProcessorDay(window_limit.effective_day()),
          utils::CreateTime(window_limit.starts_at().hour(),
                            window_limit.starts_at().minute()),
          utils::CreateTime(window_limit.ends_at().hour(),
                            window_limit.ends_at().minute()),
          last_updated);
    }
  }

  /* End Window Limits data */

  return policy;
}

ConsistencyGoldenOutput ConvertProcessorOutputToGoldenOutput(
    usage_time_limit::State state) {
  ConsistencyGoldenOutput golden_output;

  golden_output.set_is_locked(state.is_locked);
  golden_output.set_active_policy(
      ConvertProcessorPolicyToGoldenPolicy(state.active_policy));

  if (state.is_locked) {
    golden_output.set_next_unlocking_time_millis(
        state.next_unlock_time.ToJavaTime());
  }

  return golden_output;
}

}  // namespace time_limit_consistency
}  // namespace chromeos
