// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_policy.h"

#include <memory>

#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

const StorableSource::SourceType kSourceTypes[] = {
    StorableSource::SourceType::kNavigation,
    StorableSource::SourceType::kEvent,
};

class ConfigurableAttributionPolicy : public AttributionPolicy {
 public:
  explicit ConfigurableAttributionPolicy(bool should_noise)
      : should_noise_(should_noise) {}

 protected:
  bool ShouldNoiseTriggerData() const override { return should_noise_; }

  uint64_t MakeNoisedTriggerData(uint64_t max) const override { return 1; }

 private:
  bool should_noise_;
};

}  // namespace

TEST(AttributionPolicyTest, HighEntropyTriggerData_StrippedToLowerBits) {
  std::unique_ptr<AttributionPolicy> policy =
      std::make_unique<ConfigurableAttributionPolicy>(/*should_noise=*/false);

  EXPECT_EQ(0u, policy->SanitizeTriggerData(
                    8, StorableSource::SourceType::kNavigation));
  EXPECT_EQ(1u, policy->SanitizeTriggerData(
                    9, StorableSource::SourceType::kNavigation));

  EXPECT_EQ(0u,
            policy->SanitizeTriggerData(2, StorableSource::SourceType::kEvent));
  EXPECT_EQ(1u,
            policy->SanitizeTriggerData(3, StorableSource::SourceType::kEvent));
}

TEST(AttributionPolicyTest, SanitizeHighEntropySourceEventId_Unchanged) {
  uint64_t source_event_id = 256LU;

  // The policy should not alter the impression data, and return the base 10
  // representation.
  EXPECT_EQ(256LU, AttributionPolicy().SanitizeSourceEventId(source_event_id));
}

TEST(AttributionPolicyTest, LowEntropyTriggerData_Unchanged) {
  std::unique_ptr<AttributionPolicy> policy =
      std::make_unique<ConfigurableAttributionPolicy>(/*should_noise=*/false);

  for (uint64_t trigger_data = 0; trigger_data < 8; trigger_data++) {
    EXPECT_EQ(trigger_data,
              policy->SanitizeTriggerData(
                  trigger_data, StorableSource::SourceType::kNavigation));
  }
  for (uint64_t trigger_data = 0; trigger_data < 2; trigger_data++) {
    EXPECT_EQ(trigger_data,
              policy->SanitizeTriggerData(trigger_data,
                                          StorableSource::SourceType::kEvent));
  }
}

TEST(AttributionPolicyTest, SanitizeTriggerData_OutputHasNoise) {
  // The policy should include noise when sanitizing data.
  for (auto source_type : kSourceTypes) {
    EXPECT_EQ(1LU, ConfigurableAttributionPolicy(/*should_noise=*/true)
                       .SanitizeTriggerData(0UL, source_type));
  }
}

// This test will fail flakily if noise is used.
TEST(AttributionPolicyTest, DebugMode_TriggerDataNotNoised) {
  const uint64_t trigger_data = 0UL;
  for (auto source_type : kSourceTypes) {
    for (int i = 0; i < 100; i++) {
      EXPECT_EQ(trigger_data,
                AttributionPolicy(/*debug_mode=*/true)
                    .SanitizeTriggerData(trigger_data, source_type));
    }
  }
}

TEST(AttributionPolicyTest, NoExpiryForImpression_DefaultUsed) {
  const base::Time impression_time = base::Time::Now();

  for (auto source_type : kSourceTypes) {
    EXPECT_EQ(
        impression_time + base::Days(30),
        AttributionPolicy().GetExpiryTimeForImpression(
            /*declared_expiry=*/absl::nullopt, impression_time, source_type));
  }
}

TEST(AttributionPolicyTest, LargeImpressionExpirySpecified_ClampedTo30Days) {
  constexpr base::TimeDelta declared_expiry = base::Days(60);
  const base::Time impression_time = base::Time::Now();

  for (auto source_type : kSourceTypes) {
    EXPECT_EQ(impression_time + base::Days(30),
              AttributionPolicy().GetExpiryTimeForImpression(
                  declared_expiry, impression_time, source_type));
  }
}

TEST(AttributionPolicyTest, SmallImpressionExpirySpecified_ClampedTo1Day) {
  const struct {
    base::TimeDelta declared_expiry;
    base::TimeDelta want_expiry;
  } kTestCases[] = {
      {base::Days(-1), base::Days(1)},
      {base::Days(0), base::Days(1)},
      {base::Days(1) - base::Milliseconds(1), base::Days(1)},
  };

  const base::Time impression_time = base::Time::Now();

  for (auto source_type : kSourceTypes) {
    for (const auto& test_case : kTestCases) {
      EXPECT_EQ(impression_time + test_case.want_expiry,
                AttributionPolicy().GetExpiryTimeForImpression(
                    test_case.declared_expiry, impression_time, source_type));
    }
  }
}

TEST(AttributionPolicyTest, NonWholeDayImpressionExpirySpecified_Rounded) {
  const struct {
    StorableSource::SourceType source_type;
    base::TimeDelta declared_expiry;
    base::TimeDelta want_expiry;
  } kTestCases[] = {
      {StorableSource::SourceType::kNavigation, base::Hours(36),
       base::Hours(36)},
      {StorableSource::SourceType::kEvent, base::Hours(36), base::Days(2)},

      {StorableSource::SourceType::kNavigation,
       base::Days(1) + base::Milliseconds(1),
       base::Days(1) + base::Milliseconds(1)},
      {StorableSource::SourceType::kEvent,
       base::Days(1) + base::Milliseconds(1), base::Days(1)},
  };

  const base::Time impression_time = base::Time::Now();

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(
        impression_time + test_case.want_expiry,
        AttributionPolicy().GetExpiryTimeForImpression(
            test_case.declared_expiry, impression_time, test_case.source_type));
  }
}

TEST(AttributionPolicyTest, ImpressionExpirySpecified_ExpiryOverrideDefault) {
  constexpr base::TimeDelta declared_expiry = base::Days(10);
  const base::Time impression_time = base::Time::Now();

  for (auto source_type : kSourceTypes) {
    EXPECT_EQ(impression_time + base::Days(10),
              AttributionPolicy().GetExpiryTimeForImpression(
                  declared_expiry, impression_time, source_type));
  }
}

TEST(AttributionPolicyTest, GetFailedReportDelay) {
  const struct {
    int failed_send_attempts;
    absl::optional<base::TimeDelta> expected;
  } kTestCases[] = {
      {1, base::Minutes(5)},
      {2, base::Minutes(15)},
      {3, absl::nullopt},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(test_case.expected, AttributionPolicy().GetFailedReportDelay(
                                      test_case.failed_send_attempts))
        << "failed_send_attempts=" << test_case.failed_send_attempts;
  }
}

}  // namespace content
