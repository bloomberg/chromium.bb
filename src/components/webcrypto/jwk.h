// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBCRYPTO_JWK_H_
#define COMPONENTS_WEBCRYPTO_JWK_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/values.h"
#include "third_party/blink/public/platform/web_crypto.h"

namespace webcrypto {

class CryptoData;
class Status;

// Helper class for parsing a JWK from JSON.
//
// This primarily exists to ensure strict enforcement of the JWK schema, as the
// type and presence of particular members is security relevant. For example,
// GetString() will ensure a given JSON member is present and is a string type,
// and will fail if these conditions aren't met.
//
// Users of JwkReader must call Init() successfully before any other method can
// be called.
class JwkReader {
 public:
  JwkReader();
  ~JwkReader();

  // Initializes a JWK reader by parsing the JSON |bytes|. To succeed, the JWK
  // must:
  //   * Have "kty" matching |expected_kty|
  //   * Have "ext" compatible with |expected_extractable|
  //   * Have usages ("use", "key_ops") compatible with |expected_usages|
  //   * Have an "alg" matching |expected_alg|
  //
  // NOTE: If |expected_alg| is empty, then the test on "alg" is skipped.
  Status Init(const CryptoData& bytes,
              bool expected_extractable,
              blink::WebCryptoKeyUsageMask expected_usages,
              const std::string& expected_kty,
              const std::string& expected_alg);

  // Returns true if the member |member_name| is present.
  bool HasMember(const std::string& member_name) const;

  // Extracts the required string member |member_name| and saves the result to
  // |*result|. If the member does not exist or is not a string, returns an
  // error.
  Status GetString(const std::string& member_name, std::string* result) const;

  // Extracts the optional string member |member_name| and saves the result to
  // |*result| if it was found. If the member exists and is not a string,
  // returns an error. Otherwise returns success, and sets |*member_exists| if
  // it was found.
  Status GetOptionalString(const std::string& member_name,
                           std::string* result,
                           bool* member_exists) const;

  // Extracts the optional array member |member_name| and saves the result to
  // |*result| if it was found. If the member exists and is not an array,
  // returns an error. Otherwise returns success, and sets |*member_exists| if
  // it was found.
  //
  // NOTE: |*result| is owned by the JwkReader.
  Status GetOptionalList(const std::string& member_name,
                         const base::ListValue** result,
                         bool* member_exists) const;

  // Extracts the required string member |member_name| and saves the
  // base64url-decoded bytes to |*result|. If the member does not exist or is
  // not a string, or could not be base64url-decoded, returns an error.
  Status GetBytes(const std::string& member_name, std::string* result) const;

  // Extracts the required base64url member, which is interpreted as being a
  // big-endian unsigned integer.
  //
  // Sequences that contain leading zeros will be rejected.
  Status GetBigInteger(const std::string& member_name,
                       std::string* result) const;

  // Extracts the optional boolean member |member_name| and saves the result to
  // |*result| if it was found. If the member exists and is not a boolean,
  // returns an error. Otherwise returns success, and sets |*member_exists| if
  // it was found.
  Status GetOptionalBool(const std::string& member_name,
                         bool* result,
                         bool* member_exists) const;

  // Gets the optional algorithm ("alg") string.
  Status GetAlg(std::string* alg, bool* has_alg) const;

  // Checks if the "alg" member matches |expected_alg|.
  Status VerifyAlg(const std::string& expected_alg) const;

 private:
  base::DictionaryValue dict_;
};

// Helper class for building the JSON for a JWK.
class JwkWriter {
 public:
  // Initializes a writer, and sets the standard JWK members as indicated.
  // |algorithm| is optional, and is only written if the provided |algorithm| is
  // non-empty.
  JwkWriter(const std::string& algorithm,
            bool extractable,
            blink::WebCryptoKeyUsageMask usages,
            const std::string& kty);

  // Sets a string member |member_name| to |value|.
  void SetString(const std::string& member_name, const std::string& value);

  // Sets a bytes member |value| to |value| by base64 url-safe encoding it.
  void SetBytes(const std::string& member_name, const CryptoData& value);

  // Flattens the JWK to JSON (UTF-8 encoded if necessary, however in practice
  // it will be ASCII).
  void ToJson(std::vector<uint8_t>* utf8_bytes) const;

 private:
  base::DictionaryValue dict_;
};

// Converts a JWK "key_ops" array to the corresponding WebCrypto usages. Used by
// testing.
Status GetWebCryptoUsagesFromJwkKeyOpsForTest(
    const base::ListValue* key_ops,
    blink::WebCryptoKeyUsageMask* usages);

}  // namespace webcrypto

#endif  // COMPONENTS_WEBCRYPTO_JWK_H_
