// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/v2_handshake.h"
#include "base/rand_util.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "crypto/random.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/obj.h"
#include "url/gurl.h"

namespace device {
namespace cablev2 {

namespace {

TEST(CableV2Encoding, TunnelServerURLs) {
  uint8_t tunnel_id[16] = {0};
  // Tunnel ID zero should map to Google's tunnel server.
  const tunnelserver::KnownDomainID kGoogleDomain(0);
  const GURL url = tunnelserver::GetNewTunnelURL(kGoogleDomain, tunnel_id);
  EXPECT_TRUE(url.spec().find("//cable.ua5v.com/") != std::string::npos) << url;

  // The hash function shouldn't change across releases, so test a hashed
  // domain.
  const tunnelserver::KnownDomainID kHashedDomain(266);
  const GURL hashed_url =
      tunnelserver::GetNewTunnelURL(kHashedDomain, tunnel_id);
  EXPECT_TRUE(hashed_url.spec().find("//cable.wufkweyy3uaxb.com/") !=
              std::string::npos)
      << url;
}

TEST(CableV2Encoding, EIDToFromComponents) {
  eid::Components components;
  components.tunnel_server_domain = tunnelserver::KnownDomainID(0x0102);
  components.routing_id = {9, 10, 11};
  crypto::RandBytes(components.nonce);

  const CableEidArray eid = eid::FromComponents(components);
  const eid::Components components2 = eid::ToComponents(eid);

  EXPECT_EQ(components.routing_id, components2.routing_id);
  EXPECT_EQ(components.tunnel_server_domain, components2.tunnel_server_domain);
  EXPECT_EQ(components.nonce, components2.nonce);
}

TEST(CableV2Encoding, EIDEncrypt) {
  eid::Components components;
  components.tunnel_server_domain = tunnelserver::KnownDomainID(0x0102);
  components.routing_id = {9, 10, 11};
  crypto::RandBytes(components.nonce);
  const CableEidArray eid = eid::FromComponents(components);

  uint8_t key[kEIDKeySize];
  crypto::RandBytes(key);
  std::array<uint8_t, kAdvertSize> advert = eid::Encrypt(eid, key);

  const absl::optional<CableEidArray> eid2 = eid::Decrypt(advert, key);
  ASSERT_TRUE(eid2.has_value());
  EXPECT_TRUE(memcmp(eid.data(), eid2->data(), eid.size()) == 0);

  advert[0] ^= 1;
  EXPECT_FALSE(eid::Decrypt(advert, key).has_value());

  // Unknown tunnel server domains should not decrypt.
  components.tunnel_server_domain = tunnelserver::KnownDomainID(255);
  const CableEidArray eid3 = eid::FromComponents(components);
  std::array<uint8_t, kAdvertSize> invalid_advert = eid::Encrypt(eid3, key);
  EXPECT_FALSE(eid::Decrypt(invalid_advert, key).has_value());
}

TEST(CableV2Encoding, QRs) {
  std::array<uint8_t, kQRKeySize> qr_key;
  crypto::RandBytes(qr_key);
  std::string url = qr::Encode(qr_key);
  const absl::optional<qr::Components> decoded = qr::Parse(url);
  ASSERT_TRUE(decoded.has_value()) << url;
  static_assert(EXTENT(qr_key) >= EXTENT(decoded->secret), "");
  EXPECT_EQ(memcmp(decoded->secret.data(),
                   &qr_key[qr_key.size() - decoded->secret.size()],
                   decoded->secret.size()),
            0);

  url[0] ^= 4;
  EXPECT_FALSE(qr::Parse(url));
  EXPECT_FALSE(qr::Parse("nonsense"));
}

TEST(CableV2Encoding, PaddedCBOR) {
  cbor::Value::MapValue map1;
  absl::optional<std::vector<uint8_t>> encoded =
      EncodePaddedCBORMap(std::move(map1));
  ASSERT_TRUE(encoded);
  EXPECT_EQ(kPostHandshakeMsgPaddingGranularity, encoded->size());

  absl::optional<cbor::Value> decoded = DecodePaddedCBORMap(*encoded);
  ASSERT_TRUE(decoded);
  EXPECT_EQ(0u, decoded->GetMap().size());

  cbor::Value::MapValue map2;
  uint8_t blob[kPostHandshakeMsgPaddingGranularity] = {0};
  map2.emplace(1, base::span<const uint8_t>(blob, sizeof(blob)));
  encoded = EncodePaddedCBORMap(std::move(map2));
  ASSERT_TRUE(encoded);
  EXPECT_EQ(kPostHandshakeMsgPaddingGranularity * 2, encoded->size());

  decoded = DecodePaddedCBORMap(*encoded);
  ASSERT_TRUE(decoded);
  EXPECT_EQ(1u, decoded->GetMap().size());
}

// FutureEncodePaddedCBORMapFunction is the future replacement for
// |EncodePaddedCBORMap|. See comment on |DecodePaddedCBORMap16|.
absl::optional<std::vector<uint8_t>> FutureEncodePaddedCBORMapFunction(
    cbor::Value::MapValue map) {
  // TODO: when promoting this function, update comment on
  // |kPostHandshakeMsgPaddingGranularity|.

  // The number of padding bytes is a uint16_t, so the granularity cannot be
  // larger than that.
  static_assert(kFuturePostHandshakeMsgPaddingGranularity > 0, "");
  static_assert(kFuturePostHandshakeMsgPaddingGranularity - 1 <=
                    std::numeric_limits<uint16_t>::max(),
                "");

  absl::optional<std::vector<uint8_t>> cbor_bytes =
      cbor::Writer::Write(cbor::Value(std::move(map)));
  if (!cbor_bytes) {
    return absl::nullopt;
  }

  base::CheckedNumeric<size_t> padded_size_checked = cbor_bytes->size();
  padded_size_checked += sizeof(uint16_t);  // padding-length bytes
  padded_size_checked =
      (padded_size_checked + kFuturePostHandshakeMsgPaddingGranularity - 1) &
      ~(kFuturePostHandshakeMsgPaddingGranularity - 1);
  if (!padded_size_checked.IsValid()) {
    return absl::nullopt;
  }

  const size_t padded_size = padded_size_checked.ValueOrDie();
  DCHECK_GE(padded_size, cbor_bytes->size() + sizeof(uint16_t));
  const size_t extra_bytes = padded_size - cbor_bytes->size();
  const size_t num_padding_bytes =
      extra_bytes - sizeof(uint16_t) /* length of padding length */;

  cbor_bytes->resize(padded_size);
  const uint16_t num_padding_bytes16 =
      base::checked_cast<uint16_t>(num_padding_bytes);
  memcpy(&cbor_bytes.value()[padded_size - sizeof(num_padding_bytes16)],
         &num_padding_bytes16, sizeof(num_padding_bytes16));

  return *cbor_bytes;
}

TEST(CableV2Encoding, FuturePaddedCBOR) {
  // Test that we can decode messages padded by the encoding function that
  // will be used in the future.
  for (size_t i = 0; i < 512; i++) {
    SCOPED_TRACE(i);

    // Check that new->old direction works.
    const std::vector<uint8_t> dummy_array(i);
    cbor::Value::MapValue map;
    map.emplace(1, dummy_array);
    absl::optional<std::vector<uint8_t>> encoded =
        FutureEncodePaddedCBORMapFunction(std::move(map));
    ASSERT_TRUE(encoded);

    absl::optional<cbor::Value> decoded = DecodePaddedCBORMap(*encoded);
    ASSERT_TRUE(decoded);
  }
}

std::array<uint8_t, kP256X962Length> PublicKeyOf(const EC_KEY* private_key) {
  std::array<uint8_t, kP256X962Length> ret;
  CHECK_EQ(ret.size(),
           EC_POINT_point2oct(EC_KEY_get0_group(private_key),
                              EC_KEY_get0_public_key(private_key),
                              POINT_CONVERSION_UNCOMPRESSED, ret.data(),
                              ret.size(), /*ctx=*/nullptr));
  return ret;
}

TEST(CableV2Encoding, Digits) {
  uint8_t test_data[24];
  base::RandBytes(test_data, sizeof(test_data));

  // |BytesToDigits| and |DigitsToBytes| should round-trip.
  for (size_t i = 0; i < sizeof(test_data); i++) {
    std::string digits =
        qr::BytesToDigits(base::span<const uint8_t>(test_data, i));
    absl::optional<std::vector<uint8_t>> test_data_again =
        qr::DigitsToBytes(digits);
    ASSERT_TRUE(test_data_again.has_value());
    ASSERT_EQ(test_data_again->size(), i);
    ASSERT_EQ(0, memcmp(test_data_again->data(), test_data, i));
  }

  // |DigitsToBytes| should reject non-digit inputs.
  EXPECT_FALSE(qr::DigitsToBytes("a"));
  EXPECT_FALSE(qr::DigitsToBytes("ab"));
  EXPECT_FALSE(qr::DigitsToBytes("abc"));

  // |DigitsToBytes| should reject digits that can't be correct. Here three
  // digits translates into one byte, but 999 > 0xff.
  EXPECT_FALSE(qr::DigitsToBytes("999"));

  // |DigitsToBytes| should reject impossible input lengths.
  char digits[20];
  memset(digits, '0', sizeof(digits));
  for (size_t i = 0; i < sizeof(digits); i++) {
    absl::optional<std::vector<uint8_t>> bytes =
        qr::DigitsToBytes(base::StringPiece(digits, i));
    if (!bytes.has_value()) {
      continue;
    }
    EXPECT_TRUE(std::all_of(bytes->begin(), bytes->end(),
                            [](uint8_t v) -> bool { return v == 0; }));
  }

  // The encoding is used as part of an external protocol and so should not
  // change.
  static const uint8_t kTestBytes[3] = {'a', 'b', 255};
  EXPECT_EQ(qr::BytesToDigits(kTestBytes), "16736865");
}

TEST(CableV2Encoding, HandshakeSignatures) {
  static const uint8_t kSeed0[kQRSeedSize] = {0};
  static const uint8_t kSeed1[kQRSeedSize] = {1};

  bssl::UniquePtr<EC_GROUP> group(
      EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
  bssl::UniquePtr<EC_KEY> authenticator_key(
      EC_KEY_derive_from_secret(group.get(), kSeed0, sizeof(kSeed0)));
  bssl::UniquePtr<EC_KEY> client_key(
      EC_KEY_derive_from_secret(group.get(), kSeed1, sizeof(kSeed1)));

  const std::array<uint8_t, kP256X962Length> authenticator_public_key =
      PublicKeyOf(authenticator_key.get());
  const std::array<uint8_t, kP256X962Length> client_public_key =
      PublicKeyOf(client_key.get());

  HandshakeHash handshake_hash = {1};

  std::vector<uint8_t> signature = CalculatePairingSignature(
      authenticator_key.get(), client_public_key, handshake_hash);
  EXPECT_TRUE(VerifyPairingSignature(kSeed1, authenticator_public_key,
                                     handshake_hash, signature));

  handshake_hash[0] ^= 1;
  EXPECT_FALSE(VerifyPairingSignature(kSeed1, authenticator_public_key,
                                      handshake_hash, signature));
  handshake_hash[0] ^= 1;

  signature[0] ^= 1;
  EXPECT_FALSE(VerifyPairingSignature(kSeed1, authenticator_public_key,
                                      handshake_hash, signature));
  signature[0] ^= 1;
}

class CableV2HandshakeTest : public ::testing::Test {
 public:
  CableV2HandshakeTest() {
    std::fill(psk_.begin(), psk_.end(), 0);
    std::fill(identity_seed_.begin(), identity_seed_.end(), 2);

    bssl::UniquePtr<EC_GROUP> group(
        EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
    identity_key_.reset(EC_KEY_derive_from_secret(
        group.get(), identity_seed_.data(), identity_seed_.size()));
    CHECK_EQ(identity_public_.size(),
             EC_POINT_point2oct(
                 group.get(), EC_KEY_get0_public_key(identity_key_.get()),
                 POINT_CONVERSION_UNCOMPRESSED, identity_public_.data(),
                 identity_public_.size(), /*ctx=*/nullptr));
  }

 protected:
  std::array<uint8_t, 32> psk_;
  bssl::UniquePtr<EC_KEY> identity_key_;
  std::array<uint8_t, kP256X962Length> identity_public_;
  std::array<uint8_t, kQRSeedSize> identity_seed_;
};

TEST_F(CableV2HandshakeTest, MessageEncrytion) {
  std::array<uint8_t, 32> key1, key2;
  std::fill(key1.begin(), key1.end(), 1);
  std::fill(key2.begin(), key2.end(), 2);

  Crypter a(key1, key2);
  Crypter b(key2, key1);

  static constexpr size_t kMaxSize = 530;
  std::vector<uint8_t> message, ciphertext, plaintext;
  message.reserve(kMaxSize);
  ciphertext.reserve(kMaxSize);
  plaintext.reserve(kMaxSize);

  for (size_t i = 0; i < kMaxSize; i++) {
    ciphertext = message;
    ASSERT_TRUE(a.Encrypt(&ciphertext));
    ASSERT_TRUE(b.Decrypt(ciphertext, &plaintext));
    ASSERT_TRUE(plaintext == message);

    ciphertext[(13 * i) % ciphertext.size()] ^= 1;
    ASSERT_FALSE(b.Decrypt(ciphertext, &plaintext));

    message.push_back(i & 0xff);
  }
}

TEST_F(CableV2HandshakeTest, NKHandshake) {
  std::array<uint8_t, 32> wrong_psk = psk_;
  wrong_psk[0] ^= 1;

  for (const bool use_correct_key : {false, true}) {
    HandshakeInitiator initiator(use_correct_key ? psk_ : wrong_psk,
                                 identity_public_,
                                 /*identity_seed=*/absl::nullopt);
    std::vector<uint8_t> message = initiator.BuildInitialMessage();
    std::vector<uint8_t> response;
    EC_KEY_up_ref(identity_key_.get());
    HandshakeResult responder_result(RespondToHandshake(
        psk_, bssl::UniquePtr<EC_KEY>(identity_key_.get()),
        /*peer_identity=*/absl::nullopt, message, &response));
    ASSERT_EQ(responder_result.has_value(), use_correct_key);
    if (!use_correct_key) {
      continue;
    }

    absl::optional<std::pair<std::unique_ptr<Crypter>, HandshakeHash>>
        initiator_result(initiator.ProcessResponse(response));
    ASSERT_TRUE(initiator_result.has_value());
    EXPECT_EQ(initiator_result->second, responder_result->second);
    EXPECT_TRUE(responder_result->first->IsCounterpartyOfForTesting(
        *initiator_result->first));
    EXPECT_EQ(initiator_result->second, responder_result->second);
  }
}

TEST_F(CableV2HandshakeTest, KNHandshake) {
  std::array<uint8_t, kQRSeedSize> wrong_seed;
  crypto::RandBytes(wrong_seed);

  for (const bool use_correct_key : {false, true}) {
    SCOPED_TRACE(use_correct_key);

    base::span<const uint8_t, kQRSeedSize> seed =
        use_correct_key ? identity_seed_ : wrong_seed;
    HandshakeInitiator initiator(psk_,
                                 /*peer_identity=*/absl::nullopt, seed);
    std::vector<uint8_t> message = initiator.BuildInitialMessage();
    std::vector<uint8_t> response;
    HandshakeResult responder_result(RespondToHandshake(
        psk_,
        /*identity=*/nullptr, identity_public_, message, &response));
    ASSERT_EQ(responder_result.has_value(), use_correct_key);

    if (!use_correct_key) {
      continue;
    }

    absl::optional<std::pair<std::unique_ptr<Crypter>, HandshakeHash>>
        initiator_result(initiator.ProcessResponse(response));
    ASSERT_TRUE(initiator_result.has_value());
    EXPECT_TRUE(responder_result->first->IsCounterpartyOfForTesting(
        *initiator_result->first));
    EXPECT_EQ(initiator_result->second, responder_result->second);
  }
}

}  // namespace
}  // namespace cablev2
}  // namespace device
