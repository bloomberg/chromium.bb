// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/authentication_method.h"

#include "base/base64.h"
#include "base/logging.h"
#include "crypto/hmac.h"
#include "remoting/protocol/auth_util.h"

namespace remoting {
namespace protocol {

// static
AuthenticationMethod AuthenticationMethod::Invalid() {
  return AuthenticationMethod();
}

// static
AuthenticationMethod AuthenticationMethod::Spake2(HashFunction hash_function) {
  return AuthenticationMethod(SPAKE2, hash_function);
}

// static
AuthenticationMethod AuthenticationMethod::Spake2Pair() {
  return AuthenticationMethod(SPAKE2_PAIR, HMAC_SHA256);
}

// static
AuthenticationMethod AuthenticationMethod::ThirdParty() {
  return AuthenticationMethod(THIRD_PARTY, NONE);
}

// static
AuthenticationMethod AuthenticationMethod::FromString(
    const std::string& value) {
  if (value == "spake2_pair") {
    return Spake2Pair();
  } else if (value == "spake2_plain") {
    return Spake2(NONE);
  } else if (value == "spake2_hmac") {
    return Spake2(HMAC_SHA256);
  } else if (value == "third_party") {
    return ThirdParty();
  } else {
    return AuthenticationMethod::Invalid();
  }
}

// static
std::string AuthenticationMethod::ApplyHashFunction(
    HashFunction hash_function,
    const std::string& tag,
    const std::string& shared_secret) {
  switch (hash_function) {
    case NONE:
      return shared_secret;
      break;

    case HMAC_SHA256: {
      crypto::HMAC response(crypto::HMAC::SHA256);
      if (!response.Init(tag)) {
        LOG(FATAL) << "HMAC::Init failed";
      }

      unsigned char out_bytes[kSharedSecretHashLength];
      if (!response.Sign(shared_secret, out_bytes, sizeof(out_bytes))) {
        LOG(FATAL) << "HMAC::Sign failed";
      }

      return std::string(out_bytes, out_bytes + sizeof(out_bytes));
    }
  }

  NOTREACHED();
  return shared_secret;
}

AuthenticationMethod::AuthenticationMethod()
    : type_(INVALID),
      hash_function_(NONE) {
}

AuthenticationMethod::AuthenticationMethod(MethodType type,
                                           HashFunction hash_function)
    : type_(type),
      hash_function_(hash_function) {
  DCHECK_NE(type_, INVALID);
}

AuthenticationMethod::HashFunction AuthenticationMethod::hash_function() const {
  DCHECK(is_valid());
  return hash_function_;
}

const std::string AuthenticationMethod::ToString() const {
  DCHECK(is_valid());

  switch (type_) {
    case INVALID:
      NOTREACHED();
      break;

    case SPAKE2_PAIR:
      return "spake2_pair";

    case SPAKE2:
      switch (hash_function_) {
        case NONE:
          return "spake2_plain";
        case HMAC_SHA256:
          return "spake2_hmac";
      }
      break;

    case THIRD_PARTY:
      return "third_party";
  }

  return "invalid";
}

bool AuthenticationMethod::operator ==(
    const AuthenticationMethod& other) const {
  return type_ == other.type_ &&
      hash_function_ == other.hash_function_;
}

bool SharedSecretHash::Parse(const std::string& as_string) {
  size_t separator = as_string.find(':');
  if (separator == std::string::npos)
    return false;

  std::string function_name = as_string.substr(0, separator);
  if (function_name == "plain") {
    hash_function = AuthenticationMethod::NONE;
  } else if (function_name == "hmac") {
    hash_function = AuthenticationMethod::HMAC_SHA256;
  } else {
    return false;
  }

  if (!base::Base64Decode(as_string.substr(separator + 1), &value)) {
    return false;
  }

  return true;
}

}  // namespace protocol
}  // namespace remoting
