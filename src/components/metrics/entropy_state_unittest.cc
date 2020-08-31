// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/entropy_state.h"

#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

class EntropyStateTest : public testing::Test {
 public:
  EntropyStateTest() { MetricsService::RegisterPrefs(prefs_.registry()); }

  EntropyStateTest(const EntropyStateTest&) = delete;
  EntropyStateTest& operator=(const EntropyStateTest&) = delete;

 protected:
  TestingPrefServiceSimple prefs_;
};

TEST_F(EntropyStateTest, LowEntropySource0NotReset) {
  EntropyState entropy_state(&prefs_);
  // Get the low entropy source once, to initialize it.
  entropy_state.GetLowEntropySource();

  // Now, set it to 0 and ensure it doesn't get reset.
  entropy_state.low_entropy_source_ = 0;
  EXPECT_EQ(0, entropy_state.GetLowEntropySource());
  // Call it another time, just to make sure.
  EXPECT_EQ(0, entropy_state.GetLowEntropySource());
}

TEST_F(EntropyStateTest, HaveNoLowEntropySource) {
  EntropyState entropy_state(&prefs_);
  // If we have neither the new nor old low entropy sources in prefs, then the
  // new source should be created...
  int new_low_source = entropy_state.GetLowEntropySource();
  EXPECT_TRUE(EntropyState::IsValidLowEntropySource(new_low_source))
      << new_low_source;
  // ...but the old source should not...
  EXPECT_EQ(EntropyState::kLowEntropySourceNotSet,
            entropy_state.GetOldLowEntropySource());
  // ...and the high entropy source should include the *new* low entropy source.
  std::string high_source = entropy_state.GetHighEntropySource(
      "AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEF");
  EXPECT_TRUE(base::EndsWith(high_source, base::NumberToString(new_low_source),
                             base::CompareCase::SENSITIVE))
      << high_source;
}

TEST_F(EntropyStateTest, HaveOnlyNewLowEntropySource) {
  // If we have the new low entropy sources in prefs, but not the old one...
  const int new_low_source = 1234;
  prefs_.SetInteger(prefs::kMetricsLowEntropySource, new_low_source);

  EntropyState entropy_state(&prefs_);
  // ...then the new source should be loaded...
  EXPECT_EQ(new_low_source, entropy_state.GetLowEntropySource());
  // ...but the old source should not be created...
  EXPECT_EQ(EntropyState::kLowEntropySourceNotSet,
            entropy_state.GetOldLowEntropySource());
  // ...and the high entropy source should include the *new* low entropy source.
  std::string high_source = entropy_state.GetHighEntropySource(
      "AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEF");
  EXPECT_TRUE(base::EndsWith(high_source, base::NumberToString(new_low_source),
                             base::CompareCase::SENSITIVE))
      << high_source;
}

TEST_F(EntropyStateTest, HaveOnlyOldLowEntropySource) {
  // If we have the old low entropy sources in prefs, but not the new one...
  const int old_low_source = 5678;
  prefs_.SetInteger(prefs::kMetricsOldLowEntropySource, old_low_source);

  // ...then the new source should be created...
  EntropyState entropy_state(&prefs_);

  int new_low_source = entropy_state.GetLowEntropySource();
  EXPECT_TRUE(EntropyState::IsValidLowEntropySource(new_low_source))
      << new_low_source;
  // ...and the old source should be loaded...
  EXPECT_EQ(old_low_source, entropy_state.GetOldLowEntropySource());
  // ...and the high entropy source should include the *old* low entropy source.
  std::string high_source = entropy_state.GetHighEntropySource(
      "AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEF");
  EXPECT_TRUE(base::EndsWith(high_source, base::NumberToString(old_low_source),
                             base::CompareCase::SENSITIVE))
      << high_source;
}

TEST_F(EntropyStateTest, HaveBothLowEntropySources) {
  // If we have the new and old low entropy sources in prefs...
  const int new_low_source = 1234;
  const int old_low_source = 5678;
  prefs_.SetInteger(prefs::kMetricsLowEntropySource, new_low_source);
  prefs_.SetInteger(prefs::kMetricsOldLowEntropySource, old_low_source);

  // ...then both should be loaded...
  EntropyState entropy_state(&prefs_);

  EXPECT_EQ(new_low_source, entropy_state.GetLowEntropySource());
  EXPECT_EQ(old_low_source, entropy_state.GetOldLowEntropySource());
  // ...and the high entropy source should include the *old* low entropy source.
  std::string high_source = entropy_state.GetHighEntropySource(
      "AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEF");
  EXPECT_TRUE(base::EndsWith(high_source, base::NumberToString(old_low_source),
                             base::CompareCase::SENSITIVE))
      << high_source;
}

TEST_F(EntropyStateTest, CorruptNewLowEntropySources) {
  EntropyState entropy_state(&prefs_);
  const int corrupt_sources[] = {-12345, -1, 8000, 12345};
  for (int corrupt_source : corrupt_sources) {
    // If the new low entropy source has been corrupted...
    EXPECT_FALSE(EntropyState::IsValidLowEntropySource(corrupt_source))
        << corrupt_source;
    prefs_.SetInteger(prefs::kMetricsLowEntropySource, corrupt_source);
    // ...then a new source should be created.
    int loaded_source = entropy_state.GetLowEntropySource();
    EXPECT_TRUE(EntropyState::IsValidLowEntropySource(loaded_source))
        << loaded_source;
  }
}

TEST_F(EntropyStateTest, CorruptOldLowEntropySources) {
  EntropyState entropy_state(&prefs_);
  const int corrupt_sources[] = {-12345, -1, 8000, 12345};
  for (int corrupt_source : corrupt_sources) {
    // If the old low entropy source has been corrupted...
    EXPECT_FALSE(EntropyState::IsValidLowEntropySource(corrupt_source))
        << corrupt_source;
    prefs_.SetInteger(prefs::kMetricsOldLowEntropySource, corrupt_source);
    // ...then it should be ignored.
    EXPECT_EQ(EntropyState::kLowEntropySourceNotSet,
              entropy_state.GetOldLowEntropySource());
  }
}

}  // namespace metrics
