// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_AUTH_TOKEN_UTILS_H_
#define REMOTING_PROTOCOL_AUTH_TOKEN_UTILS_H_

#include <string>

namespace remoting {
namespace protocol {

// Generates auth token for the specified |jid| and |access_code|.
std::string GenerateSupportAuthToken(const std::string& jid,
                                     const std::string& access_code);

// Verifies validity of an |access_token|.
bool VerifySupportAuthToken(const std::string& jid,
                            const std::string& access_code,
                            const std::string& auth_token);

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_AUTH_TOKEN_UTILS_H_
