// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_key_commitments.h"

#include "base/base64.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_command_line.h"
#include "base/test/task_environment.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/mojom/trust_tokens.mojom-forward.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/trust_tokens/suitable_trust_token_origin.h"
#include "services/network/trust_tokens/trust_token_parameterization.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

mojom::TrustTokenKeyCommitmentResultPtr GetCommitmentForOrigin(
    const TrustTokenKeyCommitments& commitments,
    const url::Origin& origin) {
  mojom::TrustTokenKeyCommitmentResultPtr result;
  bool ran = false;

  commitments.Get(origin, base::BindLambdaForTesting(
                              [&](mojom::TrustTokenKeyCommitmentResultPtr ptr) {
                                result = std::move(ptr);
                                ran = true;
                              }));

  CHECK(ran);
  return result;
}

TEST(TrustTokenKeyCommitments, Empty) {
  TrustTokenKeyCommitments commitments;

  // We shouldn't find any commitments in an empty store.
  EXPECT_FALSE(GetCommitmentForOrigin(
      commitments,
      url::Origin::Create(GURL("https://suitable-origin.example"))));
}

TEST(TrustTokenKeyCommitments, CantRetrieveRecordForUnsuitableOrigin) {
  TrustTokenKeyCommitments commitments;

  // Opaque origins are insecure, and, consequently, not suitable for use a
  // Trust Tokens issuer origins; so, the |Set| call should decline to store the
  // result.
  base::flat_map<url::Origin, mojom::TrustTokenKeyCommitmentResultPtr> to_set;
  to_set.insert_or_assign(url::Origin(),
                          mojom::TrustTokenKeyCommitmentResult::New());
  commitments.Set(std::move(to_set));

  // We shouldn't find any commitment corresponding to an unsuitable origin.
  EXPECT_FALSE(GetCommitmentForOrigin(commitments, url::Origin()));
}

TEST(TrustTokenKeyCommitments, CanRetrieveRecordForSuitableOrigin) {
  TrustTokenKeyCommitments commitments;

  auto expectation = mojom::TrustTokenKeyCommitmentResult::New();
  expectation->batch_size = 5;

  auto suitable_origin = *SuitableTrustTokenOrigin::Create(
      GURL("https://suitable-origin.example"));

  // Opaque origins are insecure, and, consequently, not suitable for use a
  // Trust Tokens issuer origins; so, the |Set| call should decline to store the
  // result.
  base::flat_map<url::Origin, mojom::TrustTokenKeyCommitmentResultPtr> to_set;
  to_set.insert_or_assign(suitable_origin.origin(), expectation.Clone());
  commitments.Set(std::move(to_set));

  // We shouldn't find any commitment corresponding to an unsuitable origin.
  auto result = GetCommitmentForOrigin(commitments, suitable_origin.origin());
  ASSERT_TRUE(result);
  EXPECT_TRUE(result.Equals(expectation));
}

TEST(TrustTokenKeyCommitments, CantRetrieveRecordForOriginNotPresent) {
  TrustTokenKeyCommitments commitments;

  auto an_origin =
      *SuitableTrustTokenOrigin::Create(GURL("https://an-origin.example"));
  auto an_expectation = mojom::TrustTokenKeyCommitmentResult::New();
  an_expectation->batch_size = 5;

  base::flat_map<url::Origin, mojom::TrustTokenKeyCommitmentResultPtr> to_set;
  to_set.insert_or_assign(an_origin.origin(), an_expectation.Clone());
  commitments.Set(std::move(to_set));

  auto another_origin =
      *SuitableTrustTokenOrigin::Create(GURL("https://another-origin.example"));

  // We shouldn't find any commitment corresponding to an origin not in the map.
  EXPECT_FALSE(GetCommitmentForOrigin(commitments, another_origin.origin()));
}

TEST(TrustTokenKeyCommitments, MultipleOrigins) {
  TrustTokenKeyCommitments commitments;

  SuitableTrustTokenOrigin origins[] = {
      *SuitableTrustTokenOrigin::Create(GURL("https://an-origin.example")),
      *SuitableTrustTokenOrigin::Create(GURL("https://another-origin.example")),
  };

  mojom::TrustTokenKeyCommitmentResultPtr expectations[] = {
      mojom::TrustTokenKeyCommitmentResult::New(),
      mojom::TrustTokenKeyCommitmentResult::New(),
  };

  expectations[0]->batch_size = 0;
  expectations[1]->batch_size = 1;

  base::flat_map<url::Origin, mojom::TrustTokenKeyCommitmentResultPtr> to_set;
  to_set.insert_or_assign(origins[0].origin(), expectations[0].Clone());
  to_set.insert_or_assign(origins[1].origin(), expectations[1].Clone());
  commitments.Set(std::move(to_set));

  for (int i : {0, 1}) {
    auto result = GetCommitmentForOrigin(commitments, origins[i].origin());
    ASSERT_TRUE(result);
    EXPECT_TRUE(result.Equals(expectations[i]));
  }
}

TEST(TrustTokenKeyCommitments, ParseAndSet) {
  TrustTokenKeyCommitments commitments;
  commitments.ParseAndSet(
      R"( { "https://issuer.example": { "batchsize": 5, "srrkey": "aaaa" } } )");

  EXPECT_TRUE(GetCommitmentForOrigin(
      commitments,
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.example"))));
}

TEST(TrustTokenKeyCommitments, KeysFromCommandLine) {
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kAdditionalTrustTokenKeyCommitments,
      R"( { "https://issuer.example": { "batchsize": 5, "srrkey": "aaaa" } } )");

  TrustTokenKeyCommitments commitments;

  EXPECT_TRUE(GetCommitmentForOrigin(
      commitments,
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.example"))));

  commitments.ParseAndSet(
      R"( { "https://issuer.example": { "batchsize": 10, "srrkey": "bbbb" } } )");

  // A commitment provided through |Set| should defer to the one passed
  // through the command line.
  std::string expected_srrkey;
  ASSERT_TRUE(base::Base64Decode("aaaa", &expected_srrkey));

  auto result = GetCommitmentForOrigin(
      commitments,
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.example")));
  ASSERT_TRUE(result);
  EXPECT_EQ(result->signed_redemption_record_verification_key, expected_srrkey);
  EXPECT_EQ(result->batch_size, 5);
}

TEST(TrustTokenKeyCommitments, FiltersKeys) {
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  TrustTokenKeyCommitments commitments;

  auto origin =
      *SuitableTrustTokenOrigin::Create(GURL("https://an-origin.example"));
  auto commitment_result = mojom::TrustTokenKeyCommitmentResult::New();
  auto expired_key = mojom::TrustTokenVerificationKey::New();
  expired_key->expiry = base::Time::Now() - base::TimeDelta::FromMinutes(1);
  commitment_result->keys.push_back(std::move(expired_key));

  static_assert(kMaximumConcurrentlyValidTrustTokenVerificationKeys < 100,
                "If the constant grows large, consider rewriting this test.");
  for (size_t i = 0; i < kMaximumConcurrentlyValidTrustTokenVerificationKeys;
       ++i) {
    auto not_expired_key = mojom::TrustTokenVerificationKey::New();
    not_expired_key->expiry =
        base::Time::Now() + base::TimeDelta::FromMinutes(1);
    commitment_result->keys.push_back(std::move(not_expired_key));
  }

  auto late_to_expire_key = mojom::TrustTokenVerificationKey::New();
  late_to_expire_key->expiry =
      base::Time::Now() + base::TimeDelta::FromMinutes(2);

  // We expect to get rid of the expired key and the farthest-in-the-future key
  // (since there are more than kMaximum... many keys yet to expire).
  base::flat_map<url::Origin, mojom::TrustTokenKeyCommitmentResultPtr> to_set;
  to_set.insert_or_assign(origin.origin(), commitment_result.Clone());
  commitments.Set(std::move(to_set));

  auto result = GetCommitmentForOrigin(commitments, origin);
  EXPECT_EQ(
      result->keys.size(),
      static_cast<size_t>(kMaximumConcurrentlyValidTrustTokenVerificationKeys));
  EXPECT_TRUE(std::all_of(result->keys.begin(), result->keys.end(),
                          [](const mojom::TrustTokenVerificationKeyPtr& key) {
                            return key->expiry ==
                                   base::Time::Now() +
                                       base::TimeDelta::FromMinutes(1);
                          }));
}

TEST(TrustTokenKeyCommitments, GetSync) {
  TrustTokenKeyCommitments commitments;

  auto expectation = mojom::TrustTokenKeyCommitmentResult::New();
  expectation->batch_size = 5;

  auto suitable_origin = *SuitableTrustTokenOrigin::Create(
      GURL("https://suitable-origin.example"));

  base::flat_map<url::Origin, mojom::TrustTokenKeyCommitmentResultPtr> to_set;
  to_set.insert_or_assign(suitable_origin.origin(), expectation.Clone());
  commitments.Set(std::move(to_set));

  auto result = commitments.GetSync(suitable_origin.origin());
  ASSERT_TRUE(result);
  EXPECT_TRUE(result.Equals(expectation));
}

}  // namespace network
