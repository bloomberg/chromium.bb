// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/origin_trials/origin_trials.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {
namespace {

const char kFrobulateTrialName[] = "Frobulate";
const char kFrobulateDeprecationTrialName[] = "FrobulateDeprecation";
const char kFrobulateThirdPartyTrialName[] = "FrobulateThirdParty";

}  // namespace

TEST(OriginTrialTest, TrialsValid) {
  EXPECT_TRUE(origin_trials::IsTrialValid(kFrobulateTrialName));
  EXPECT_TRUE(origin_trials::IsTrialValid(kFrobulateThirdPartyTrialName));
}

TEST(OriginTrialTest, TrialEnabledForInsecureContext) {
  EXPECT_FALSE(
      origin_trials::IsTrialEnabledForInsecureContext(kFrobulateTrialName));
  EXPECT_TRUE(origin_trials::IsTrialEnabledForInsecureContext(
      kFrobulateDeprecationTrialName));
  EXPECT_FALSE(origin_trials::IsTrialEnabledForInsecureContext(
      kFrobulateThirdPartyTrialName));
}

TEST(OriginTrialTest, TrialsEnabledForThirdPartyOrigins) {
  EXPECT_FALSE(
      origin_trials::IsTrialEnabledForThirdPartyOrigins(kFrobulateTrialName));
  EXPECT_TRUE(origin_trials::IsTrialEnabledForThirdPartyOrigins(
      kFrobulateThirdPartyTrialName));
}

}  // namespace blink
