// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/interest_group/interest_group_mojom_traits.h"

#include "base/time/time.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {

namespace {

const char kOrigin1[] = "https://origin1.test/";
const char kOrigin2[] = "https://origin2.test/";

const char kName1[] = "Name 1";
const char kName2[] = "Name 2";

// Two URLs that share kOrigin1.
const char kUrl1[] = "https://origin1.test/url1";
const char kUrl2[] = "https://origin1.test/url2";

// Creates an InterestGroup with an owner and a name,which are mandatory fields.
InterestGroup CreateInterestGroup() {
  InterestGroup interest_group;
  interest_group.owner = url::Origin::Create(GURL(kOrigin1));
  interest_group.name = kName1;
  return interest_group;
}

// SerializesAndDeserializes the provided interest group, expecting
// deserialization to succeed. Expects the deserialization to succeed, and to be
// the same as the original group. Also makes sure the input InterestGroup is
// not equal to the output of CreateInterestGroup(), to verify that
// IsEqualForTesting() is checking whatever was modified in the input group.
//
// Arguments is not const because SerializeAndDeserialize() doesn't take a
// const input value, as serializing some object types is destructive.
void SerializeAndDeserializeAndCompare(InterestGroup& interest_group) {
  ASSERT_FALSE(interest_group.IsEqualForTesting(CreateInterestGroup()));

  InterestGroup interest_group_clone;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<blink::mojom::InterestGroup>(
      interest_group, interest_group_clone));
  EXPECT_TRUE(interest_group.IsEqualForTesting(interest_group_clone));
}

}  // namespace

// This file has tests for the deserialization success case. Failure cases are
// currently tested alongside ValidateBlinkInterestGroup(), since their failure
// cases should be the same.

TEST(InterestGroupMojomTraitsTest, SerializeAndDeserializeExpiry) {
  InterestGroup interest_group = CreateInterestGroup();
  interest_group.expiry = base::Time::Now();
  SerializeAndDeserializeAndCompare(interest_group);
}

TEST(InterestGroupMojomTraitsTest, SerializeAndDeserializeOwner) {
  InterestGroup interest_group = CreateInterestGroup();
  interest_group.owner = url::Origin::Create(GURL(kOrigin2));
  SerializeAndDeserializeAndCompare(interest_group);
}

TEST(InterestGroupMojomTraitsTest, SerializeAndDeserializeName) {
  InterestGroup interest_group = CreateInterestGroup();
  interest_group.name = kName2;
  SerializeAndDeserializeAndCompare(interest_group);
}

TEST(InterestGroupMojomTraitsTest, SerializeAndDeserializeBiddingUrl) {
  InterestGroup interest_group = CreateInterestGroup();
  interest_group.bidding_url = GURL(kUrl1);
  SerializeAndDeserializeAndCompare(interest_group);
}

TEST(InterestGroupMojomTraitsTest, SerializeAndDeserializeUpdateUrl) {
  InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = GURL(kUrl1);
  SerializeAndDeserializeAndCompare(interest_group);
}

TEST(InterestGroupMojomTraitsTest,
     SerializeAndDeserializeTrustedBiddingSignalsUrl) {
  InterestGroup interest_group = CreateInterestGroup();
  interest_group.trusted_bidding_signals_url = GURL(kUrl1);
  SerializeAndDeserializeAndCompare(interest_group);
}

TEST(InterestGroupMojomTraitsTest,
     SerializeAndDeserializeTrustedBiddingSignalsKeys) {
  InterestGroup interest_group = CreateInterestGroup();
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->emplace_back("foo");
  SerializeAndDeserializeAndCompare(interest_group);
}

TEST(InterestGroupMojomTraitsTest, SerializeAndDeserializeUserBiddingSignals) {
  InterestGroup interest_group = CreateInterestGroup();
  interest_group.user_bidding_signals = "[]";
  SerializeAndDeserializeAndCompare(interest_group);
}

TEST(InterestGroupMojomTraitsTest, SerializeAndDeserializeAds) {
  InterestGroup interest_group = CreateInterestGroup();
  interest_group.ads.emplace();
  interest_group.ads->emplace_back(
      InterestGroup::Ad(GURL(kUrl1), /*metadata=*/absl::nullopt));
  interest_group.ads->emplace_back(
      InterestGroup::Ad(GURL(kUrl2), /*metadata=*/"[]"));
  SerializeAndDeserializeAndCompare(interest_group);
}

TEST(InterestGroupMojomTraitsTest, SerializeAndDeserializeAdComponents) {
  InterestGroup interest_group = CreateInterestGroup();
  interest_group.ad_components.emplace();
  interest_group.ad_components->emplace_back(
      InterestGroup::Ad(GURL(kUrl1), /*metadata=*/absl::nullopt));
  interest_group.ad_components->emplace_back(
      InterestGroup::Ad(GURL(kUrl2), /*metadata=*/"[]"));
  SerializeAndDeserializeAndCompare(interest_group);
}

}  // namespace blink
