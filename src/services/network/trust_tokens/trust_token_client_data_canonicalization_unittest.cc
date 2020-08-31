// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_client_data_canonicalization.h"

#include "base/containers/span.h"
#include "base/strings/string_piece.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/cbor/reader.h"
#include "crypto/sha2.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

TEST(TrustTokenClientDataCanonicalization, TimeBeforeUnixEpoch) {
  EXPECT_FALSE(CanonicalizeTrustTokenClientDataForRedemption(
      base::Time::UnixEpoch() - base::TimeDelta::FromSeconds(1),
      url::Origin::Create(GURL("https://topframe.example")), "public key"));
}

TEST(TrustTokenClientDataCanonicalization, SerializeThenDeserialize) {
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  base::Optional<std::vector<uint8_t>> maybe_serialization =
      CanonicalizeTrustTokenClientDataForRedemption(
          base::Time::Now(),
          url::Origin::Create(GURL("https://topframe.example")), "public key");

  ASSERT_TRUE(maybe_serialization);

  base::Optional<cbor::Value> maybe_deserialized_cbor =
      cbor::Reader::Read(base::make_span(*maybe_serialization));

  ASSERT_TRUE(maybe_deserialized_cbor);
  ASSERT_TRUE(maybe_deserialized_cbor->is_map());

  const cbor::Value::MapValue& map = maybe_deserialized_cbor->GetMap();

  ASSERT_EQ(
      map.at(cbor::Value("redemption-timestamp", cbor::Value::Type::STRING))
          .GetUnsigned(),
      (base::Time::Now() - base::Time::UnixEpoch()).InSeconds());

  ASSERT_EQ(map.at(cbor::Value("redeeming-origin", cbor::Value::Type::STRING))
                .GetString(),
            "https://topframe.example");

  ASSERT_EQ(map.at(cbor::Value("key-hash", cbor::Value::Type::STRING))
                .GetBytestringAsString(),
            crypto::SHA256HashString("public key"));
}

}  // namespace network
