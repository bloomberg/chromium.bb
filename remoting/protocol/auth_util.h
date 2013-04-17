// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_AUTH_UTIL_H_
#define REMOTING_PROTOCOL_AUTH_UTIL_H_

#include <string>

#include "base/strings/string_piece.h"

namespace net {
class SSLSocket;
}  // namespace net

namespace remoting {
namespace protocol {

// Labels for use when exporting the SSL master keys.
extern const char kClientAuthSslExporterLabel[];
extern const char kHostAuthSslExporterLabel[];

// Fake hostname used for SSL connections.
extern const char kSslFakeHostName[];

// Size of the HMAC-SHA-256 hash used as shared secret in SPAKE2.
const size_t kSharedSecretHashLength = 32;

// Size of the HMAC-SHA-256 digest used for channel authentication.
const size_t kAuthDigestLength = 32;

// TODO(sergeyu): The following two methods are used for V1
// authentication. Remove them when we finally switch to V2
// authentication method. crbug.com/110483 .

// Generates auth token for the specified |jid| and |access_code|.
std::string GenerateSupportAuthToken(const std::string& jid,
                                     const std::string& access_code);

// Verifies validity of an |access_token|.
bool VerifySupportAuthToken(const std::string& jid,
                            const std::string& access_code,
                            const std::string& auth_token);

// Returns authentication bytes that must be used for the given
// |socket|. Empty string is returned in case of failure.
std::string GetAuthBytes(net::SSLSocket* socket,
                         const base::StringPiece& label,
                         const base::StringPiece& shared_secret);

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_AUTH_UTIL_H_
