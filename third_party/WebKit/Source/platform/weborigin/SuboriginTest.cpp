// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/weborigin/Suborigin.h"

#include "platform/wtf/dtoa/utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

bool PolicyContainsOnly(const Suborigin& suborigin,
                        const Suborigin::SuboriginPolicyOptions options[],
                        size_t options_size) {
  unsigned options_mask = 0;
  for (size_t i = 0; i < options_size; i++)
    options_mask |= static_cast<unsigned>(options[i]);

  return options_mask == suborigin.OptionsMask();
}

TEST(SuboriginTest, PolicyTests) {
  Suborigin suborigin_unsafe_postmessage_send;
  Suborigin suborigin_unsafe_postmessage_receive;
  Suborigin suborigin_all_unsafe_postmessage;

  const Suborigin::SuboriginPolicyOptions kEmptyPolicies[] = {
      Suborigin::SuboriginPolicyOptions::kNone};
  const Suborigin::SuboriginPolicyOptions kUnsafePostmessageSendPolicy[] = {
      Suborigin::SuboriginPolicyOptions::kUnsafePostMessageSend};
  const Suborigin::SuboriginPolicyOptions kUnsafePostmessageReceivePolicy[] = {
      Suborigin::SuboriginPolicyOptions::kUnsafePostMessageReceive};
  const Suborigin::SuboriginPolicyOptions kAllUnsafePostmessagePolicies[] = {
      Suborigin::SuboriginPolicyOptions::kUnsafePostMessageSend,
      Suborigin::SuboriginPolicyOptions::kUnsafePostMessageReceive};

  EXPECT_TRUE(PolicyContainsOnly(suborigin_unsafe_postmessage_send,
                                 kEmptyPolicies, ARRAY_SIZE(kEmptyPolicies)));

  suborigin_unsafe_postmessage_send.AddPolicyOption(
      Suborigin::SuboriginPolicyOptions::kUnsafePostMessageSend);
  EXPECT_TRUE(PolicyContainsOnly(suborigin_unsafe_postmessage_send,
                                 kUnsafePostmessageSendPolicy,
                                 ARRAY_SIZE(kUnsafePostmessageSendPolicy)));
  EXPECT_FALSE(PolicyContainsOnly(suborigin_unsafe_postmessage_send,
                                  kAllUnsafePostmessagePolicies,
                                  ARRAY_SIZE(kAllUnsafePostmessagePolicies)));

  suborigin_unsafe_postmessage_receive.AddPolicyOption(
      Suborigin::SuboriginPolicyOptions::kUnsafePostMessageReceive);
  EXPECT_TRUE(PolicyContainsOnly(suborigin_unsafe_postmessage_receive,
                                 kUnsafePostmessageReceivePolicy,
                                 ARRAY_SIZE(kUnsafePostmessageReceivePolicy)));
  EXPECT_FALSE(PolicyContainsOnly(suborigin_unsafe_postmessage_receive,
                                  kAllUnsafePostmessagePolicies,
                                  ARRAY_SIZE(kAllUnsafePostmessagePolicies)));

  suborigin_all_unsafe_postmessage.AddPolicyOption(
      Suborigin::SuboriginPolicyOptions::kUnsafePostMessageSend);
  suborigin_all_unsafe_postmessage.AddPolicyOption(
      Suborigin::SuboriginPolicyOptions::kUnsafePostMessageReceive);
  EXPECT_TRUE(PolicyContainsOnly(suborigin_all_unsafe_postmessage,
                                 kAllUnsafePostmessagePolicies,
                                 ARRAY_SIZE(kAllUnsafePostmessagePolicies)));
}

}  // namespace blink
