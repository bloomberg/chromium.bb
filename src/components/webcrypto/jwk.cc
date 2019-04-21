// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webcrypto/jwk.h"

#include <stddef.h>

#include <set>

#include "base/base64url.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "components/webcrypto/algorithms/util.h"
#include "components/webcrypto/crypto_data.h"
#include "components/webcrypto/status.h"

// JSON Web Key Format (JWK) is defined by:
// http://tools.ietf.org/html/draft-ietf-jose-json-web-key
//
// A JWK is a simple JSON dictionary with the following members:
// - "kty" (Key Type) Parameter, REQUIRED
// - <kty-specific parameters, see below>, REQUIRED
// - "use" (Key Use) OPTIONAL
// - "key_ops" (Key Operations) OPTIONAL
// - "alg" (Algorithm) OPTIONAL
// - "ext" (Key Exportability), OPTIONAL
// (all other entries are ignored)
//
// The <kty-specific parameters> are defined by the JWA spec:
// http://tools.ietf.org/html/draft-ietf-jose-json-web-algorithms

namespace webcrypto {

namespace {

// Web Crypto equivalent usage mask for JWK 'use' = 'enc'.
const blink::WebCryptoKeyUsageMask kJwkEncUsage =
    blink::kWebCryptoKeyUsageEncrypt | blink::kWebCryptoKeyUsageDecrypt |
    blink::kWebCryptoKeyUsageWrapKey | blink::kWebCryptoKeyUsageUnwrapKey;
// Web Crypto equivalent usage mask for JWK 'use' = 'sig'.
const blink::WebCryptoKeyUsageMask kJwkSigUsage =
    blink::kWebCryptoKeyUsageSign | blink::kWebCryptoKeyUsageVerify;

// Checks that the "ext" member of the JWK is consistent with
// "expected_extractable".
Status VerifyExt(const JwkReader& jwk, bool expected_extractable) {
  // JWK "ext" (optional) --> extractable parameter
  bool jwk_ext_value = false;
  bool has_jwk_ext;
  Status status = jwk.GetOptionalBool("ext", &jwk_ext_value, &has_jwk_ext);
  if (status.IsError())
    return status;
  if (has_jwk_ext && expected_extractable && !jwk_ext_value)
    return Status::ErrorJwkExtInconsistent();
  return Status::Success();
}

struct JwkToWebCryptoUsageMapping {
  const char* const jwk_key_op;
  const blink::WebCryptoKeyUsage webcrypto_usage;
};

// Keep this ordered the same as WebCrypto's "recognized key usage
// values". While this is not required for spec compliance,
// it makes the ordering of key_ops match that of WebCrypto's Key.usages.
const JwkToWebCryptoUsageMapping kJwkWebCryptoUsageMap[] = {
    {"encrypt", blink::kWebCryptoKeyUsageEncrypt},
    {"decrypt", blink::kWebCryptoKeyUsageDecrypt},
    {"sign", blink::kWebCryptoKeyUsageSign},
    {"verify", blink::kWebCryptoKeyUsageVerify},
    {"deriveKey", blink::kWebCryptoKeyUsageDeriveKey},
    {"deriveBits", blink::kWebCryptoKeyUsageDeriveBits},
    {"wrapKey", blink::kWebCryptoKeyUsageWrapKey},
    {"unwrapKey", blink::kWebCryptoKeyUsageUnwrapKey}};

bool JwkKeyOpToWebCryptoUsage(const std::string& key_op,
                              blink::WebCryptoKeyUsage* usage) {
  for (size_t i = 0; i < base::size(kJwkWebCryptoUsageMap); ++i) {
    if (kJwkWebCryptoUsageMap[i].jwk_key_op == key_op) {
      *usage = kJwkWebCryptoUsageMap[i].webcrypto_usage;
      return true;
    }
  }
  return false;
}

// Creates a JWK key_ops list from a Web Crypto usage mask.
std::unique_ptr<base::ListValue> CreateJwkKeyOpsFromWebCryptoUsages(
    blink::WebCryptoKeyUsageMask usages) {
  std::unique_ptr<base::ListValue> jwk_key_ops(new base::ListValue());
  for (size_t i = 0; i < base::size(kJwkWebCryptoUsageMap); ++i) {
    if (usages & kJwkWebCryptoUsageMap[i].webcrypto_usage)
      jwk_key_ops->AppendString(kJwkWebCryptoUsageMap[i].jwk_key_op);
  }
  return jwk_key_ops;
}

// Composes a Web Crypto usage mask from an array of JWK key_ops values.
Status GetWebCryptoUsagesFromJwkKeyOps(const base::ListValue* key_ops,
                                       blink::WebCryptoKeyUsageMask* usages) {
  // This set keeps track of all unrecognized key_ops values.
  std::set<std::string> unrecognized_usages;

  *usages = 0;
  for (size_t i = 0; i < key_ops->GetSize(); ++i) {
    std::string key_op;
    if (!key_ops->GetString(i, &key_op)) {
      return Status::ErrorJwkMemberWrongType(
          base::StringPrintf("key_ops[%d]", static_cast<int>(i)), "string");
    }

    blink::WebCryptoKeyUsage usage;
    if (JwkKeyOpToWebCryptoUsage(key_op, &usage)) {
      // Ensure there are no duplicate usages.
      if (*usages & usage)
        return Status::ErrorJwkDuplicateKeyOps();
      *usages |= usage;
    }

    // Reaching here means the usage was unrecognized. Such usages are skipped
    // over, however they are kept track of in a set to ensure there were no
    // duplicates.
    if (!unrecognized_usages.insert(key_op).second)
      return Status::ErrorJwkDuplicateKeyOps();
  }
  return Status::Success();
}

// Checks that the usages ("use" and "key_ops") of the JWK is consistent with
// "expected_usages".
Status VerifyUsages(const JwkReader& jwk,
                    blink::WebCryptoKeyUsageMask expected_usages) {
  // JWK "key_ops" (optional) --> usages parameter
  const base::ListValue* jwk_key_ops_value = nullptr;
  bool has_jwk_key_ops;
  Status status =
      jwk.GetOptionalList("key_ops", &jwk_key_ops_value, &has_jwk_key_ops);
  if (status.IsError())
    return status;
  blink::WebCryptoKeyUsageMask jwk_key_ops_mask = 0;
  if (has_jwk_key_ops) {
    status =
        GetWebCryptoUsagesFromJwkKeyOps(jwk_key_ops_value, &jwk_key_ops_mask);
    if (status.IsError())
      return status;
    // The input usages must be a subset of jwk_key_ops_mask.
    if (!ContainsKeyUsages(jwk_key_ops_mask, expected_usages))
      return Status::ErrorJwkKeyopsInconsistent();
  }

  // JWK "use" (optional) --> usages parameter
  std::string jwk_use_value;
  bool has_jwk_use;
  status = jwk.GetOptionalString("use", &jwk_use_value, &has_jwk_use);
  if (status.IsError())
    return status;
  blink::WebCryptoKeyUsageMask jwk_use_mask = 0;
  if (has_jwk_use) {
    if (jwk_use_value == "enc")
      jwk_use_mask = kJwkEncUsage;
    else if (jwk_use_value == "sig")
      jwk_use_mask = kJwkSigUsage;
    else
      return Status::ErrorJwkUnrecognizedUse();
    // The input usages must be a subset of jwk_use_mask.
    if (!ContainsKeyUsages(jwk_use_mask, expected_usages))
      return Status::ErrorJwkUseInconsistent();
  }

  // If both 'key_ops' and 'use' are present, ensure they are consistent.
  if (has_jwk_key_ops && has_jwk_use &&
      !ContainsKeyUsages(jwk_use_mask, jwk_key_ops_mask))
    return Status::ErrorJwkUseAndKeyopsInconsistent();

  return Status::Success();
}

}  // namespace

JwkReader::JwkReader() {
}

JwkReader::~JwkReader() {
}

Status JwkReader::Init(const CryptoData& bytes,
                       bool expected_extractable,
                       blink::WebCryptoKeyUsageMask expected_usages,
                       const std::string& expected_kty,
                       const std::string& expected_alg) {
  // Parse the incoming JWK JSON.
  base::StringPiece json_string(reinterpret_cast<const char*>(bytes.bytes()),
                                bytes.byte_length());

  {
    // Limit the visibility for |value| as it is moved to |dict_| (via
    // |dict_value|) once it has been loaded successfully.
    base::Optional<base::Value> value = base::JSONReader::Read(json_string);
    base::DictionaryValue* dict_value = nullptr;

    if (!value.has_value() || !value.value().GetAsDictionary(&dict_value))
      return Status::ErrorJwkNotDictionary();

    dict_ = std::move(*dict_value);
  }

  // JWK "kty". Exit early if this required JWK parameter is missing.
  std::string kty;
  Status status = GetString("kty", &kty);
  if (status.IsError())
    return status;

  if (kty != expected_kty)
    return Status::ErrorJwkUnexpectedKty(expected_kty);

  status = VerifyExt(*this, expected_extractable);
  if (status.IsError())
    return status;

  status = VerifyUsages(*this, expected_usages);
  if (status.IsError())
    return status;

  // Verify the algorithm if an expectation was provided.
  if (!expected_alg.empty()) {
    status = VerifyAlg(expected_alg);
    if (status.IsError())
      return status;
  }

  return Status::Success();
}

bool JwkReader::HasMember(const std::string& member_name) const {
  return dict_.HasKey(member_name);
}

Status JwkReader::GetString(const std::string& member_name,
                            std::string* result) const {
  const base::Value* value = nullptr;
  if (!dict_.Get(member_name, &value))
    return Status::ErrorJwkMemberMissing(member_name);
  if (!value->GetAsString(result))
    return Status::ErrorJwkMemberWrongType(member_name, "string");
  return Status::Success();
}

Status JwkReader::GetOptionalString(const std::string& member_name,
                                    std::string* result,
                                    bool* member_exists) const {
  *member_exists = false;
  const base::Value* value = nullptr;
  if (!dict_.Get(member_name, &value))
    return Status::Success();

  if (!value->GetAsString(result))
    return Status::ErrorJwkMemberWrongType(member_name, "string");

  *member_exists = true;
  return Status::Success();
}

Status JwkReader::GetOptionalList(const std::string& member_name,
                                  const base::ListValue** result,
                                  bool* member_exists) const {
  *member_exists = false;
  const base::Value* value = nullptr;
  if (!dict_.Get(member_name, &value))
    return Status::Success();

  if (!value->GetAsList(result))
    return Status::ErrorJwkMemberWrongType(member_name, "list");

  *member_exists = true;
  return Status::Success();
}

Status JwkReader::GetBytes(const std::string& member_name,
                           std::string* result) const {
  std::string base64_string;
  Status status = GetString(member_name, &base64_string);
  if (status.IsError())
    return status;

  // The JSON web signature spec says that padding is omitted.
  // https://tools.ietf.org/html/draft-ietf-jose-json-web-signature-36#section-2
  if (!base::Base64UrlDecode(base64_string,
                             base::Base64UrlDecodePolicy::DISALLOW_PADDING,
                             result)) {
    return Status::ErrorJwkBase64Decode(member_name);
  }

  return Status::Success();
}

Status JwkReader::GetBigInteger(const std::string& member_name,
                                std::string* result) const {
  Status status = GetBytes(member_name, result);
  if (status.IsError())
    return status;

  if (result->empty())
    return Status::ErrorJwkEmptyBigInteger(member_name);

  // The JWA spec says that "The octet sequence MUST utilize the minimum number
  // of octets to represent the value." This means there shouldn't be any
  // leading zeros.
  if (result->size() > 1 && (*result)[0] == 0)
    return Status::ErrorJwkBigIntegerHasLeadingZero(member_name);

  return Status::Success();
}

Status JwkReader::GetOptionalBool(const std::string& member_name,
                                  bool* result,
                                  bool* member_exists) const {
  *member_exists = false;
  const base::Value* value = nullptr;
  if (!dict_.Get(member_name, &value))
    return Status::Success();

  if (!value->GetAsBoolean(result))
    return Status::ErrorJwkMemberWrongType(member_name, "boolean");

  *member_exists = true;
  return Status::Success();
}

Status JwkReader::GetAlg(std::string* alg, bool* has_alg) const {
  return GetOptionalString("alg", alg, has_alg);
}

Status JwkReader::VerifyAlg(const std::string& expected_alg) const {
  bool has_jwk_alg;
  std::string jwk_alg_value;
  Status status = GetAlg(&jwk_alg_value, &has_jwk_alg);
  if (status.IsError())
    return status;

  if (has_jwk_alg && jwk_alg_value != expected_alg)
    return Status::ErrorJwkAlgorithmInconsistent();

  return Status::Success();
}

JwkWriter::JwkWriter(const std::string& algorithm,
                     bool extractable,
                     blink::WebCryptoKeyUsageMask usages,
                     const std::string& kty) {
  if (!algorithm.empty())
    dict_.SetString("alg", algorithm);
  dict_.Set("key_ops", CreateJwkKeyOpsFromWebCryptoUsages(usages));
  dict_.SetBoolean("ext", extractable);
  dict_.SetString("kty", kty);
}

void JwkWriter::SetString(const std::string& member_name,
                          const std::string& value) {
  dict_.SetString(member_name, value);
}

void JwkWriter::SetBytes(const std::string& member_name,
                         const CryptoData& value) {
  // The JSON web signature spec says that padding is omitted.
  // https://tools.ietf.org/html/draft-ietf-jose-json-web-signature-36#section-2
  std::string base64url_encoded;
  base::Base64UrlEncode(
      base::StringPiece(reinterpret_cast<const char*>(value.bytes()),
                        value.byte_length()),
      base::Base64UrlEncodePolicy::OMIT_PADDING, &base64url_encoded);

  dict_.SetString(member_name, base64url_encoded);
}

void JwkWriter::ToJson(std::vector<uint8_t>* utf8_bytes) const {
  std::string json;
  base::JSONWriter::Write(dict_, &json);
  utf8_bytes->assign(json.begin(), json.end());
}

Status GetWebCryptoUsagesFromJwkKeyOpsForTest(
    const base::ListValue* key_ops,
    blink::WebCryptoKeyUsageMask* usages) {
  return GetWebCryptoUsagesFromJwkKeyOps(key_ops, usages);
}

}  // namespace webcrypto
