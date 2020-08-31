// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_store.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/trust_tokens/in_memory_trust_token_persister.h"
#include "services/network/trust_tokens/proto/public.pb.h"
#include "services/network/trust_tokens/proto/storage.pb.h"
#include "services/network/trust_tokens/suitable_trust_token_origin.h"
#include "services/network/trust_tokens/trust_token_parameterization.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using ::testing::ElementsAre;
using ::testing::Optional;

namespace network {
namespace trust_tokens {

namespace {
MATCHER_P(EqualsProto,
          message,
          "Match a proto Message equal to the matcher's argument.") {
  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}
}  // namespace

TEST(TrustTokenStoreTest, RecordsIssuances) {
  // A newly initialized store should not think it's
  // recorded any issuances.

  auto my_store = TrustTokenStore::CreateForTesting(
      std::make_unique<InMemoryTrustTokenPersister>());
  SuitableTrustTokenOrigin issuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com"));
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  EXPECT_EQ(my_store->TimeSinceLastIssuance(issuer), base::nullopt);

  // Recording an issuance should result in the time
  // since last issuance being correctly returned.

  my_store->RecordIssuance(issuer);
  auto delta = base::TimeDelta::FromSeconds(1);
  env.AdvanceClock(delta);

  EXPECT_THAT(my_store->TimeSinceLastIssuance(issuer), Optional(delta));
}

TEST(TrustTokenStoreTest, DoesntReportMissingOrMalformedIssuanceTimestamps) {
  auto my_persister = std::make_unique<InMemoryTrustTokenPersister>();
  auto* raw_persister = my_persister.get();

  auto my_store = TrustTokenStore::CreateForTesting(std::move(my_persister));
  SuitableTrustTokenOrigin issuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com"));

  auto issuer_config_with_no_time = std::make_unique<TrustTokenIssuerConfig>();
  raw_persister->SetIssuerConfig(issuer, std::move(issuer_config_with_no_time));

  EXPECT_EQ(my_store->TimeSinceLastIssuance(issuer), base::nullopt);

  auto issuer_config_with_malformed_time =
      std::make_unique<TrustTokenIssuerConfig>();
  issuer_config_with_malformed_time->set_last_issuance(
      "not a valid serialization of a base::Time");
  raw_persister->SetIssuerConfig(issuer,
                                 std::move(issuer_config_with_malformed_time));

  EXPECT_EQ(my_store->TimeSinceLastIssuance(issuer), base::nullopt);
}

TEST(TrustTokenStoreTest, DoesntReportNegativeTimeSinceLastIssuance) {
  auto my_persister = std::make_unique<InMemoryTrustTokenPersister>();
  auto* raw_persister = my_persister.get();
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  auto my_store = TrustTokenStore::CreateForTesting(std::move(my_persister));
  SuitableTrustTokenOrigin issuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com"));
  base::Time later_than_now =
      base::Time::Now() + base::TimeDelta::FromSeconds(1);

  auto issuer_config_with_future_time =
      std::make_unique<TrustTokenIssuerConfig>();
  issuer_config_with_future_time->set_last_issuance(
      internal::TimeToString(later_than_now));
  raw_persister->SetIssuerConfig(issuer,
                                 std::move(issuer_config_with_future_time));

  // TimeSinceLastIssuance shouldn't return negative values.

  EXPECT_EQ(my_store->TimeSinceLastIssuance(issuer), base::nullopt);
}

TEST(TrustTokenStore, RecordsRedemptions) {
  // A newly initialized store should not think it's
  // recorded any redemptions.

  auto my_store = TrustTokenStore::CreateForTesting(
      std::make_unique<InMemoryTrustTokenPersister>());
  SuitableTrustTokenOrigin issuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com"));
  SuitableTrustTokenOrigin toplevel =
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com"));
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  EXPECT_EQ(my_store->TimeSinceLastRedemption(issuer, toplevel), base::nullopt);

  // Recording a redemption should result in the time
  // since last redemption being correctly returned.

  my_store->RecordRedemption(issuer, toplevel);
  auto delta = base::TimeDelta::FromSeconds(1);
  env.AdvanceClock(delta);

  EXPECT_THAT(my_store->TimeSinceLastRedemption(issuer, toplevel),
              Optional(delta));
}

TEST(TrustTokenStoreTest, DoesntReportMissingOrMalformedRedemptionTimestamps) {
  auto my_persister = std::make_unique<InMemoryTrustTokenPersister>();
  auto* raw_persister = my_persister.get();

  auto my_store = TrustTokenStore::CreateForTesting(std::move(my_persister));
  SuitableTrustTokenOrigin issuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com"));
  SuitableTrustTokenOrigin toplevel =
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com"));

  auto config_with_no_time =
      std::make_unique<TrustTokenIssuerToplevelPairConfig>();
  raw_persister->SetIssuerToplevelPairConfig(issuer, toplevel,
                                             std::move(config_with_no_time));

  EXPECT_EQ(my_store->TimeSinceLastRedemption(issuer, toplevel), base::nullopt);

  auto config_with_malformed_time =
      std::make_unique<TrustTokenIssuerToplevelPairConfig>();
  config_with_malformed_time->set_last_redemption(
      "not a valid serialization of a base::Time");
  raw_persister->SetIssuerToplevelPairConfig(
      issuer, toplevel, std::move(config_with_malformed_time));

  EXPECT_EQ(my_store->TimeSinceLastRedemption(issuer, toplevel), base::nullopt);
}

TEST(TrustTokenStoreTest, DoesntReportNegativeTimeSinceLastRedemption) {
  auto my_persister = std::make_unique<InMemoryTrustTokenPersister>();
  auto* raw_persister = my_persister.get();
  auto my_store = TrustTokenStore::CreateForTesting(std::move(my_persister));
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  SuitableTrustTokenOrigin issuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com"));
  SuitableTrustTokenOrigin toplevel =
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com"));

  base::Time later_than_now =
      base::Time::Now() + base::TimeDelta::FromSeconds(1);

  auto config_with_future_time =
      std::make_unique<TrustTokenIssuerToplevelPairConfig>();
  config_with_future_time->set_last_redemption(
      internal::TimeToString(later_than_now));

  raw_persister->SetIssuerToplevelPairConfig(
      issuer, toplevel, std::move(config_with_future_time));

  // TimeSinceLastRedemption shouldn't return negative values.

  EXPECT_EQ(my_store->TimeSinceLastRedemption(issuer, toplevel), base::nullopt);
}

TEST(TrustTokenStore, AssociatesToplevelsWithIssuers) {
  // A newly initialized store should not think
  // any toplevels are associated with any issuers.

  auto my_store = TrustTokenStore::CreateForTesting(
      std::make_unique<InMemoryTrustTokenPersister>());
  SuitableTrustTokenOrigin issuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com"));
  SuitableTrustTokenOrigin toplevel =
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com"));
  EXPECT_FALSE(my_store->IsAssociated(issuer, toplevel));

  // After associating an issuer with a toplevel,
  // the store should think that that issuer is associated
  // with that toplevel.

  EXPECT_TRUE(my_store->SetAssociation(issuer, toplevel));
  EXPECT_TRUE(my_store->IsAssociated(issuer, toplevel));
}

// Test that issuer-toplevel association works correctly when a toplevel's
// number-of-issuance cap has been reached: reassocating an already-associated
// issuer should succeed, while associating any other issuer should fail.
TEST(TrustTokenStore, IssuerToplevelAssociationAtNumberOfAssociationsCap) {
  auto persister = std::make_unique<InMemoryTrustTokenPersister>();

  SuitableTrustTokenOrigin toplevel =
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com"));
  SuitableTrustTokenOrigin issuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com"));

  auto config = std::make_unique<TrustTokenToplevelConfig>();
  for (int i = 0; i < kTrustTokenPerToplevelMaxNumberOfAssociatedIssuers - 1;
       ++i)
    config->add_associated_issuers();
  *config->add_associated_issuers() = issuer.Serialize();

  persister->SetToplevelConfig(toplevel, std::move(config));

  auto my_store = TrustTokenStore::CreateForTesting(std::move(persister));

  // Sanity check that the test set the config up correctly.
  ASSERT_TRUE(my_store->IsAssociated(issuer, toplevel));

  // Even though we're at the cap, SetAssociation for an already-associated
  // toplevel should return true.
  EXPECT_TRUE(my_store->SetAssociation(issuer, toplevel));

  // Since we're at the cap, SetAssociation for an issuer not already associated
  // with the top-level origin should fail.
  EXPECT_FALSE(my_store->SetAssociation(
      *SuitableTrustTokenOrigin::Create(GURL("https://someotherissuer.com")),
      toplevel));
}

TEST(TrustTokenStore, AddingTokensRespectsCapacity) {
  auto my_store = TrustTokenStore::CreateForTesting(
      std::make_unique<InMemoryTrustTokenPersister>());

  SuitableTrustTokenOrigin issuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com"));

  // Attempting to add many, many tokens corresponding to that key should be
  // successful, but the operation should only add a quantity of tokens equal to
  // the difference between the number of currently-stored tokens and the
  // capacity.
  my_store->AddTokens(
      issuer, std::vector<std::string>(kTrustTokenPerIssuerTokenCapacity * 2),
      /*issuing_key=*/
      "");

  EXPECT_EQ(my_store->CountTokens(issuer), kTrustTokenPerIssuerTokenCapacity);
}

TEST(TrustTokenStore, CountsTokens) {
  auto my_store = TrustTokenStore::CreateForTesting(
      std::make_unique<InMemoryTrustTokenPersister>());
  SuitableTrustTokenOrigin issuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com"));

  // A freshly initialized store should be storing zero tokens.
  EXPECT_EQ(my_store->CountTokens(issuer), 0);

  // Add a token; the count should increase.
  my_store->AddTokens(issuer, std::vector<std::string>(1),
                      /*issuing_key=*/"");
  EXPECT_EQ(my_store->CountTokens(issuer), 1);

  // Add two more tokens; the count should change accordingly.
  my_store->AddTokens(issuer, std::vector<std::string>(2),
                      /*issuing_key=*/"");
  EXPECT_EQ(my_store->CountTokens(issuer), 3);
}

TEST(TrustTokenStore, PrunesDataAssociatedWithRemovedKeyCommitments) {
  // Test that providing PruneStaleIssuerState a set of key commitments
  // correctly evicts all tokens except those associated with keys in the
  // provided set of commitments.
  auto my_store = TrustTokenStore::CreateForTesting(
      std::make_unique<InMemoryTrustTokenPersister>());
  SuitableTrustTokenOrigin issuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com"));

  my_store->AddTokens(issuer, std::vector<std::string>{"some token body"},
                      "quite a secure key, this");

  auto another_commitment = mojom::TrustTokenVerificationKey::New();
  another_commitment->body = "distinct from the first key";

  my_store->AddTokens(issuer, std::vector<std::string>{"some other token body"},
                      another_commitment->body);

  // The prune should remove the first token added, because it corresponds to a
  // key not in the list of commitments provided to PruneStaleIssuerState.
  std::vector<mojom::TrustTokenVerificationKeyPtr> keys;
  keys.emplace_back(another_commitment.Clone());
  my_store->PruneStaleIssuerState(issuer, keys);

  TrustToken expected_token;
  expected_token.set_body("some other token body");
  expected_token.set_signing_key(another_commitment->body);

  // Removing |my_commitment| should have
  // - led to the removal of the token associated with the removed key and
  // - *not* led to the removal of the token associated with the remaining key.
  EXPECT_THAT(my_store->RetrieveMatchingTokens(
                  issuer, base::BindRepeating(
                              [](const std::string& t) { return true; })),
              ElementsAre(EqualsProto(expected_token)));
}

TEST(TrustTokenStore, AddsTrustTokens) {
  // A newly initialized store should not think
  // any issuers have associated trust tokens.

  auto my_store = TrustTokenStore::CreateForTesting(
      std::make_unique<InMemoryTrustTokenPersister>());
  SuitableTrustTokenOrigin issuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com"));

  auto match_all_keys =
      base::BindRepeating([](const std::string& t) { return true; });

  EXPECT_TRUE(my_store->RetrieveMatchingTokens(issuer, match_all_keys).empty());

  // Adding a token should result in that token being
  // returned by subsequent queries with predicates accepting
  // that token.

  const std::string kMyKey = "abcdef";
  auto my_commitment = mojom::TrustTokenVerificationKey::New();
  my_commitment->body = kMyKey;

  TrustToken expected_token;
  expected_token.set_body("some token");
  expected_token.set_signing_key(kMyKey);
  my_store->AddTokens(issuer, std::vector<std::string>{expected_token.body()},
                      kMyKey);

  EXPECT_THAT(my_store->RetrieveMatchingTokens(issuer, match_all_keys),
              ElementsAre(EqualsProto(expected_token)));
}

TEST(TrustTokenStore, RetrievesTrustTokensRespectingNontrivialPredicate) {
  // RetrieveMatchingTokens should not return tokens rejected by
  // the provided predicate.

  auto my_store = TrustTokenStore::CreateForTesting(
      std::make_unique<InMemoryTrustTokenPersister>());
  SuitableTrustTokenOrigin issuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com"));

  const std::string kMatchingKey = "bbbbbb";
  const std::string kNonmatchingKey = "aaaaaa";
  auto matching_commitment = mojom::TrustTokenVerificationKey::New();
  matching_commitment->body = kMatchingKey;

  auto nonmatching_commitment = mojom::TrustTokenVerificationKey::New();
  nonmatching_commitment->body = kNonmatchingKey;

  TrustToken expected_token;
  expected_token.set_body("this one should get returned");
  expected_token.set_signing_key(kMatchingKey);

  my_store->AddTokens(issuer, std::vector<std::string>{expected_token.body()},
                      kMatchingKey);
  my_store->AddTokens(
      issuer,
      std::vector<std::string>{"this one should get rejected by the predicate"},
      kNonmatchingKey);

  EXPECT_THAT(my_store->RetrieveMatchingTokens(
                  issuer, base::BindRepeating(
                              [](const std::string& pattern,
                                 const std::string& possible_match) {
                                return possible_match == pattern;
                              },
                              kMatchingKey)),
              ElementsAre(EqualsProto(expected_token)));
}

TEST(TrustTokenStore, DeletesSingleToken) {
  auto my_store = TrustTokenStore::CreateForTesting(
      std::make_unique<InMemoryTrustTokenPersister>());
  SuitableTrustTokenOrigin issuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com"));
  auto match_all_keys =
      base::BindRepeating([](const std::string& t) { return true; });

  // Deleting a single token should result in that token
  // not being returned by subsequent RetrieveMatchingTokens calls.
  // On the other hand, tokens *not* deleted should still be
  // returned.

  auto my_commitment = mojom::TrustTokenVerificationKey::New();
  my_commitment->body = "key";

  TrustToken first_token;
  first_token.set_body("delete me!");
  first_token.set_signing_key(my_commitment->body);

  TrustToken second_token;
  second_token.set_body("don't delete me!");
  second_token.set_signing_key(my_commitment->body);

  my_store->AddTokens(
      issuer, std::vector<std::string>{first_token.body(), second_token.body()},
      my_commitment->body);

  my_store->DeleteToken(issuer, first_token);

  EXPECT_THAT(my_store->RetrieveMatchingTokens(issuer, match_all_keys),
              ElementsAre(EqualsProto(second_token)));
}

TEST(TrustTokenStore, DeleteTokenForMissingIssuer) {
  auto my_store = TrustTokenStore::CreateForTesting(
      std::make_unique<InMemoryTrustTokenPersister>());
  SuitableTrustTokenOrigin issuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com"));

  // Deletes for issuers not present in the store should gracefully no-op.

  my_store->DeleteToken(issuer, TrustToken());
}

TEST(TrustTokenStore, SetsAndRetrievesRedemptionRecord) {
  // A newly initialized store should not think
  // it has any signed redemption records.

  auto my_store = TrustTokenStore::CreateForTesting(
      std::make_unique<InMemoryTrustTokenPersister>());
  SuitableTrustTokenOrigin issuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com"));
  SuitableTrustTokenOrigin toplevel =
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com"));
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  EXPECT_EQ(my_store->RetrieveNonstaleRedemptionRecord(issuer, toplevel),
            base::nullopt);

  // Providing a redemption record should mean that subsequent
  // queries (modulo the record's staleness) should return that
  // record.

  SignedTrustTokenRedemptionRecord my_record;
  my_record.set_body("Look at me! I'm a signed redemption record!");
  my_store->SetRedemptionRecord(issuer, toplevel, my_record);

  EXPECT_THAT(my_store->RetrieveNonstaleRedemptionRecord(issuer, toplevel),
              Optional(EqualsProto(my_record)));
}

TEST(TrustTokenStore, RetrieveRedemptionRecordHandlesConfigWithNoRecord) {
  // A RetrieveRedemptionRecord call for an (issuer, toplevel) pair with
  // no redemption record stored should gracefully return the default value.

  auto my_persister = std::make_unique<InMemoryTrustTokenPersister>();
  auto* raw_persister = my_persister.get();
  auto my_store = TrustTokenStore::CreateForTesting(std::move(my_persister));
  SuitableTrustTokenOrigin issuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com"));
  SuitableTrustTokenOrigin toplevel =
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com"));

  raw_persister->SetIssuerToplevelPairConfig(
      issuer, toplevel, std::make_unique<TrustTokenIssuerToplevelPairConfig>());

  EXPECT_EQ(my_store->RetrieveNonstaleRedemptionRecord(issuer, toplevel),
            base::nullopt);
}

TEST(TrustTokenStore, SetRedemptionRecordOverwritesExisting) {
  // Subsequent redemption records should overwrite ones set earlier.

  auto my_store = TrustTokenStore::CreateForTesting(
      std::make_unique<InMemoryTrustTokenPersister>());
  SuitableTrustTokenOrigin issuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com"));
  SuitableTrustTokenOrigin toplevel =
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com"));
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  SignedTrustTokenRedemptionRecord my_record;
  my_record.set_body("Look at me! I'm a signed redemption record!");
  my_store->SetRedemptionRecord(issuer, toplevel, my_record);

  SignedTrustTokenRedemptionRecord another_record;
  another_record.set_body(
      "If all goes well, this one should overwrite |my_record|.");
  my_store->SetRedemptionRecord(issuer, toplevel, another_record);

  EXPECT_THAT(my_store->RetrieveNonstaleRedemptionRecord(issuer, toplevel),
              Optional(EqualsProto(another_record)));
}

namespace {
// Characterizes an SRR as expired if its body begins with an "a".
class LetterAExpiringExpiryDelegate
    : public TrustTokenStore::RecordExpiryDelegate {
 public:
  bool IsRecordExpired(const SignedTrustTokenRedemptionRecord& record,
                       const SuitableTrustTokenOrigin&) override {
    return record.body().size() > 1 && record.body().front() == 'a';
  }
};
}  // namespace

TEST(TrustTokenStore, DoesNotReturnStaleRedemptionRecord) {
  // Once a redemption record expires, it should no longer
  // be returned by retrieval queries.
  auto my_store = TrustTokenStore::CreateForTesting(
      std::make_unique<InMemoryTrustTokenPersister>(),
      std::make_unique<LetterAExpiringExpiryDelegate>());
  SuitableTrustTokenOrigin issuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com"));
  SuitableTrustTokenOrigin toplevel =
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com"));

  SignedTrustTokenRedemptionRecord my_record;
  my_record.set_body("aLook at me! I'm an expired signed redemption record!");
  my_store->SetRedemptionRecord(issuer, toplevel, my_record);

  EXPECT_EQ(my_store->RetrieveNonstaleRedemptionRecord(issuer, toplevel),
            base::nullopt);
}

TEST(TrustTokenStore, EmptyFilter) {
  // Deletion with an empty filter should no-op.

  auto store = TrustTokenStore::CreateForTesting();
  auto issuer = *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com"));
  store->AddTokens(issuer, std::vector<std::string>{"token"}, "key");

  EXPECT_FALSE(store->ClearDataForFilter(mojom::ClearDataFilter::New()));
  EXPECT_TRUE(store->CountTokens(issuer));
}

TEST(TrustTokenStore, EmptyFilterInKeepMatchesMode) {
  auto store = TrustTokenStore::CreateForTesting();
  auto issuer = *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com"));
  store->AddTokens(issuer, std::vector<std::string>{"token"}, "key");

  // An empty filter in mode KEEP_MATCHES should act as a wildcard "wipe the
  // store" clear.
  auto filter_matching_all_data = mojom::ClearDataFilter::New();
  filter_matching_all_data->type = mojom::ClearDataFilter::Type::KEEP_MATCHES;

  EXPECT_TRUE(store->ClearDataForFilter(std::move(filter_matching_all_data)));
  EXPECT_FALSE(store->CountTokens(issuer));
}

TEST(TrustTokenStore, ClearsIssuerKeyedData) {
  auto store = TrustTokenStore::CreateForTesting();
  auto issuer = *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com"));

  store->AddTokens(issuer, std::vector<std::string>{"token"}, "key");

  auto filter = mojom::ClearDataFilter::New();
  filter->origins.push_back(issuer);
  EXPECT_TRUE(store->ClearDataForFilter(std::move(filter)));
  EXPECT_FALSE(store->CountTokens(issuer));
}

TEST(TrustTokenStore, ClearsToplevelKeyedData) {
  auto store = TrustTokenStore::CreateForTesting();
  auto issuer = *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com"));
  auto toplevel =
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com"));

  // (The list of issuers associated with each top-level origin is keyed solely
  // by top-level origin because it is configuration set by the top-level
  // origin.)
  ASSERT_TRUE(store->SetAssociation(issuer, toplevel));
  auto filter = mojom::ClearDataFilter::New();
  filter->origins.push_back(toplevel);
  EXPECT_TRUE(store->ClearDataForFilter(std::move(filter)));
  EXPECT_FALSE(store->IsAssociated(issuer, toplevel));
}

TEST(TrustTokenStore, ClearsIssuerToplevelPairKeyedData) {
  auto store = TrustTokenStore::CreateForTesting();
  auto issuer = *SuitableTrustTokenOrigin::Create(GURL("https://issuer.com"));
  auto toplevel =
      *SuitableTrustTokenOrigin::Create(GURL("https://toplevel.com"));

  {
    store->SetRedemptionRecord(issuer, toplevel,
                               SignedTrustTokenRedemptionRecord());
    auto filter = mojom::ClearDataFilter::New();
    filter->origins.push_back(issuer);
    EXPECT_TRUE(store->ClearDataForFilter(std::move(filter)));
    EXPECT_FALSE(store->RetrieveNonstaleRedemptionRecord(issuer, toplevel));
  }

  {
    store->SetRedemptionRecord(issuer, toplevel,
                               SignedTrustTokenRedemptionRecord());
    auto filter = mojom::ClearDataFilter::New();
    filter->origins.push_back(toplevel);
    EXPECT_TRUE(store->ClearDataForFilter(std::move(filter)));
    EXPECT_FALSE(store->RetrieveNonstaleRedemptionRecord(issuer, toplevel));
  }
}

TEST(TrustTokenStore, ClearDataCanFilterByDomain) {
  auto store = TrustTokenStore::CreateForTesting();
  auto issuer = *SuitableTrustTokenOrigin::Create(
      GURL("https://arbitrary.https.subdomain.of.issuer.com"));

  store->AddTokens(issuer, std::vector<std::string>{"token"}, "key");

  auto filter = mojom::ClearDataFilter::New();
  filter->domains.push_back("issuer.com");
  EXPECT_TRUE(store->ClearDataForFilter(std::move(filter)));
  EXPECT_FALSE(store->CountTokens(issuer));
}

TEST(TrustTokenStore, RemovesDataForInvertedFilters) {
  auto store = TrustTokenStore::CreateForTesting();
  auto issuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://www.issuer.com"));
  auto toplevel =
      *SuitableTrustTokenOrigin::Create(GURL("https://www.toplevel.com"));

  store->AddTokens(issuer, std::vector<std::string>{"token"}, "key");
  store->SetRedemptionRecord(issuer, toplevel,
                             SignedTrustTokenRedemptionRecord{});

  // With a "delete all origins not covered by this filter"-type filter
  // containing just the issuer, the issuer's data shouldn't be touched, but the
  // (issuer, toplevel)-keyed data should be cleared as one origin (the
  // non-matching one) qualifies for deletion.
  auto filter = mojom::ClearDataFilter::New();
  filter->type = mojom::ClearDataFilter::Type::KEEP_MATCHES;
  filter->origins.push_back(issuer);

  EXPECT_TRUE(store->ClearDataForFilter(std::move(filter)));
  EXPECT_TRUE(store->CountTokens(issuer));
  EXPECT_FALSE(store->RetrieveNonstaleRedemptionRecord(issuer, toplevel));
}

TEST(TrustTokenStore, RemovesDataForNullFilter) {
  // A null filter is a "clear all data" wildcard.

  auto store = TrustTokenStore::CreateForTesting();
  auto issuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://www.issuer.com"));
  auto toplevel =
      *SuitableTrustTokenOrigin::Create(GURL("https://www.toplevel.com"));

  // Add some issuer-keyed state,
  store->AddTokens(issuer, std::vector<std::string>{"token"}, "key");
  // some top level-keyed state,
  ASSERT_TRUE(store->SetAssociation(issuer, toplevel));
  // and some (issuer, top level) pair-keyed state.
  store->SetRedemptionRecord(issuer, toplevel,
                             SignedTrustTokenRedemptionRecord{});

  EXPECT_TRUE(store->ClearDataForFilter(nullptr));
  EXPECT_FALSE(store->CountTokens(issuer));
  EXPECT_FALSE(store->IsAssociated(issuer, toplevel));
  EXPECT_FALSE(store->RetrieveNonstaleRedemptionRecord(issuer, toplevel));
}

}  // namespace trust_tokens
}  // namespace network
