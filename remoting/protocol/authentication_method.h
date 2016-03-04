// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef REMOTING_PROTOCOL_AUTHENTICATION_METHOD_H_
#define REMOTING_PROTOCOL_AUTHENTICATION_METHOD_H_

#include <string>

namespace remoting {
namespace protocol {

class Authenticator;

// AuthenticationMethod represents an authentication algorithm.
enum class AuthenticationMethod {
  INVALID,
  SPAKE2_SHARED_SECRET_PLAIN,
  SPAKE2_SHARED_SECRET_HMAC,
  SPAKE2_PAIR,
  THIRD_PARTY
};

enum class HashFunction {
  NONE,
  HMAC_SHA256,
};

// Parses a string that defines an authentication method. Returns
// AuthenticationMethod::INVALID if the string is invalid.
AuthenticationMethod ParseAuthenticationMethodString(const std::string& value);

// Returns string representation of |method|.
const std::string AuthenticationMethodToString(AuthenticationMethod method);

// Returns hash function applied to the shared secret on both ends for the
// spefied |method|.
HashFunction GetHashFunctionForAuthenticationMethod(
    AuthenticationMethod method);

// Applies the specified hash function to |shared_secret| with the
// specified |tag| as a key.
std::string ApplySharedSecretHashFunction(HashFunction hash_function,
                                          const std::string& tag,
                                          const std::string& shared_secret);

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_AUTHENTICATION_METHOD_H_
