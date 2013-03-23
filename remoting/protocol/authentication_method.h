// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// AuthenticationMethod represents an authentication algorithm and its
// configuration. It knows how to parse and format authentication
// method names.
// Currently the following methods are supported:
//   spake2_plain - SPAKE2 without hashing applied to the password.
//   spake2_hmac - SPAKE2 with HMAC hashing of the password.

#ifndef REMOTING_PROTOCOL_AUTHENTICATION_METHOD_H_
#define REMOTING_PROTOCOL_AUTHENTICATION_METHOD_H_

#include <string>

namespace remoting {
namespace protocol {

class Authenticator;

class AuthenticationMethod {
 public:
  enum MethodType {
    INVALID,
    SPAKE2,
    THIRD_PARTY
  };

  enum HashFunction {
    NONE,
    HMAC_SHA256,
  };

  // Constructors for various authentication methods.
  static AuthenticationMethod Invalid();
  static AuthenticationMethod Spake2(HashFunction hash_function);
  static AuthenticationMethod ThirdParty();

  // Parses a string that defines an authentication method. Returns an
  // invalid value if the string is invalid.
  static AuthenticationMethod FromString(const std::string& value);

  // Applies the specified hash function to |shared_secret| with the
  // specified |tag| as a key.
  static std::string ApplyHashFunction(HashFunction hash_function,
                                       const std::string& tag,
                                       const std::string& shared_secret);

  bool is_valid() const { return type_ != INVALID; }

  MethodType type() const { return type_; }

  // Following methods are valid only when is_valid() returns true.

  // Hash function applied to the shared secret on both ends.
  HashFunction hash_function() const;

  // Returns string representation of the value stored in this object.
  const std::string ToString() const;

  // Comparison operators so that std::find() can be used with
  // collections of this class.
  bool operator ==(const AuthenticationMethod& other) const;
  bool operator !=(const AuthenticationMethod& other) const {
    return !(*this == other);
  }

 protected:
  AuthenticationMethod();
  AuthenticationMethod(MethodType type, HashFunction hash_function);

  MethodType type_;
  HashFunction hash_function_;
};

// SharedSecretHash stores hash of a host secret paired with the type
// of the hashing function.
struct SharedSecretHash {
  AuthenticationMethod::HashFunction hash_function;
  std::string value;

  // Parse string representation of a shared secret hash. The |as_string|
  // must be in form "<hash_function>:<hash_value_base64>".
  bool Parse(const std::string& as_string);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_AUTHENTICATION_METHOD_H_
