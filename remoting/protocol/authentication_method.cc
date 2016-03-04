// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/authentication_method.h"

#include "base/logging.h"
#include "crypto/hmac.h"
#include "remoting/protocol/auth_util.h"
#include "remoting/protocol/name_value_map.h"

namespace remoting {
namespace protocol {

const NameMapElement<AuthenticationMethod> kAuthenticationMethodStrings[] = {
    {AuthenticationMethod::SPAKE2_SHARED_SECRET_PLAIN, "spake2_plain"},
    {AuthenticationMethod::SPAKE2_SHARED_SECRET_HMAC, "spake2_hmac"},
    {AuthenticationMethod::SPAKE2_PAIR, "spake2_pair"},
    {AuthenticationMethod::THIRD_PARTY, "third_party"}};

AuthenticationMethod ParseAuthenticationMethodString(const std::string& value) {
  AuthenticationMethod result;
  if (!NameToValue(kAuthenticationMethodStrings, value, &result))
    return AuthenticationMethod::INVALID;
  return result;
}

const std::string AuthenticationMethodToString(
    AuthenticationMethod method) {
  return ValueToName(kAuthenticationMethodStrings, method);
}

HashFunction GetHashFunctionForAuthenticationMethod(
    AuthenticationMethod method) {
  switch (method) {
    case AuthenticationMethod::INVALID:
      NOTREACHED();
      return HashFunction::NONE;

    case AuthenticationMethod::SPAKE2_SHARED_SECRET_PLAIN:
    case AuthenticationMethod::THIRD_PARTY:
      return HashFunction::NONE;

    case AuthenticationMethod::SPAKE2_SHARED_SECRET_HMAC:
    case AuthenticationMethod::SPAKE2_PAIR:
      return HashFunction::HMAC_SHA256;
  }

  NOTREACHED();
  return HashFunction::NONE;
}

std::string ApplySharedSecretHashFunction(HashFunction hash_function,
                                          const std::string& tag,
                                          const std::string& shared_secret) {
  switch (hash_function) {
    case HashFunction::NONE:
      return shared_secret;

    case HashFunction::HMAC_SHA256: {
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

}  // namespace protocol
}  // namespace remoting
