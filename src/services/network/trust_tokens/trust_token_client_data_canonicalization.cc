// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_client_data_canonicalization.h"

#include "components/cbor/writer.h"
#include "crypto/sha2.h"

namespace network {

namespace {

const char kRedemptionTimestampKey[] = "redemption-timestamp";
const char kRedeemingOriginKey[] = "redeeming-origin";
const char kKeyHashKey[] = "key-hash";

}  // namespace

// From the design doc:
//
// {
//    // “redemption-timestamp”’s value is of CBOR type “unsigned integer.”
//    “redemption-timestamp”: <Redemption timestamp, seconds past the Unix
//    epoch>,
//
//    // “redeeming-origin”’s value is of CBOR type “text string.”
//    “redeeming-origin”: <Top-level origin at the time of redemption>,
//
//    // “key-hash”’s value is of CBOR type “byte string.”
//    “key-hash”: SHA256(client public key)
// },
base::Optional<std::vector<uint8_t>>
CanonicalizeTrustTokenClientDataForRedemption(
    base::Time redemption_timestamp,
    const url::Origin& top_frame_origin,
    base::StringPiece public_key) {
  DCHECK(!top_frame_origin.opaque());

  cbor::Value::MapValue map;

  base::TimeDelta redemption_timestamp_minus_unix_epoch =
      redemption_timestamp - base::Time::UnixEpoch();

  if (redemption_timestamp_minus_unix_epoch < base::TimeDelta())
    return base::nullopt;

  map[cbor::Value(kRedemptionTimestampKey, cbor::Value::Type::STRING)] =
      cbor::Value(redemption_timestamp_minus_unix_epoch.InSeconds());

  map[cbor::Value(kRedeemingOriginKey, cbor::Value::Type::STRING)] =
      cbor::Value(top_frame_origin.Serialize(), cbor::Value::Type::STRING);

  map[cbor::Value(kKeyHashKey, cbor::Value::Type::STRING)] = cbor::Value(
      crypto::SHA256HashString(public_key), cbor::Value::Type::BYTE_STRING);

  return cbor::Writer::Write(cbor::Value(std::move(map)));
}

}  // namespace network
