// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/client_data_json.h"

#include "base/base64url.h"
#include "base/check.h"
#include "base/rand_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversion_utils.h"

namespace content {
namespace {

std::string Base64UrlEncode(const base::span<const uint8_t> input) {
  std::string ret;
  base::Base64UrlEncode(
      base::StringPiece(reinterpret_cast<const char*>(input.data()),
                        input.size()),
      base::Base64UrlEncodePolicy::OMIT_PADDING, &ret);
  return ret;
}

// ToJSONString encodes |in| as a JSON string, using the specific escaping rules
// required by https://github.com/w3c/webauthn/pull/1375.
std::string ToJSONString(base::StringPiece in) {
  std::string ret;
  ret.reserve(in.size() + 2);
  ret.push_back('"');

  const char* const in_bytes = in.data();
  // ICU uses |int32_t| for lengths.
  const int32_t length = base::checked_cast<int32_t>(in.size());
  int32_t offset = 0;

  while (offset < length) {
    const int32_t prior_offset = offset;
    // Input strings must be valid UTF-8.
    uint32_t codepoint;
    CHECK(base::ReadUnicodeCharacter(in_bytes, length, &offset, &codepoint));
    // offset is updated by |ReadUnicodeCharacter| to index the last byte of the
    // codepoint. Increment it to index the first byte of the next codepoint for
    // the subsequent iteration.
    offset++;

    if (codepoint == 0x20 || codepoint == 0x21 ||
        (codepoint >= 0x23 && codepoint <= 0x5b) || codepoint >= 0x5d) {
      ret.append(&in_bytes[prior_offset], &in_bytes[offset]);
    } else if (codepoint == 0x22) {
      ret.append("\\\"");
    } else if (codepoint == 0x5c) {
      ret.append("\\\\");
    } else {
      static const char hextable[17] = "0123456789abcdef";
      ret.append("\\u00");
      ret.push_back(hextable[codepoint >> 4]);
      ret.push_back(hextable[codepoint & 15]);
    }
  }

  ret.push_back('"');
  return ret;
}

}  // namespace

std::string BuildClientDataJson(
    ClientDataRequestType type,
    const std::string& origin,
    base::span<const uint8_t> challenge,
    bool is_cross_origin,
    blink::mojom::PaymentOptionsPtr payment_options /* = nullptr */,
    const std::string& payment_rp /* = "" */,
    const std::string& payment_top_origin /* = "" */) {
  std::string ret;
  ret.reserve(128);

  // U2F uses "typ", while WebAuthn uses "type" for the type key.
  switch (type) {
    case ClientDataRequestType::kU2fRegister:
      ret.append(R"({"typ":"navigator.id.finishEnrollment")");
      break;
    case ClientDataRequestType::kU2fSign:
      ret.append(R"({"typ":"navigator.id.getAssertion")");
      break;
    case ClientDataRequestType::kWebAuthnCreate:
      ret.append(R"({"type":"webauthn.create")");
      break;
    case ClientDataRequestType::kWebAuthnGet:
      ret.append(R"({"type":"webauthn.get")");
      break;
    case ClientDataRequestType::kPaymentGet:
      ret.append(R"({"type":"payment.get")");
      break;
  }

  ret.append(R"(,"challenge":)");
  ret.append(ToJSONString(Base64UrlEncode(challenge)));

  ret.append(R"(,"origin":)");
  ret.append(ToJSONString(origin));

  if (is_cross_origin) {
    ret.append(R"(,"crossOrigin":true)");
  } else {
    ret.append(R"(,"crossOrigin":false)");
  }

  if (payment_options) {
    ret.append(R"(,"payment":{)");

    ret.append(R"("rp":)");
    ret.append(ToJSONString(payment_rp));

    ret.append(R"(,"topOrigin":)");
    ret.append(ToJSONString(payment_top_origin));

    ret.append(R"(,"payeeOrigin":)");
    ret.append(ToJSONString(payment_options->payee_origin.Serialize()));

    ret.append(R"(,"total":{)");

    ret.append(R"("value":)");
    ret.append(ToJSONString(payment_options->total->value));

    ret.append(R"(,"currency":)");
    ret.append(ToJSONString(payment_options->total->currency));

    ret.append(R"(},"instrument":{)");

    ret.append(R"("icon":)");
    ret.append(ToJSONString(payment_options->instrument->icon.spec()));

    ret.append(R"(,"displayName":)");
    ret.append(ToJSONString(payment_options->instrument->display_name));

    ret.append("}}");
  }

  if (base::RandDouble() < 0.2) {
    // An extra key is sometimes added to ensure that RPs do not make
    // unreasonably specific assumptions about the clientData JSON. This is
    // done in the fashion of
    // https://tools.ietf.org/html/draft-ietf-tls-grease
    ret.append(R"(,"other_keys_can_be_added_here":")");
    ret.append(
        "do not compare clientDataJSON against a template. See "
        "https://goo.gl/yabPex\"");
  }

  ret.append("}");
  return ret;
}

}  // namespace content
