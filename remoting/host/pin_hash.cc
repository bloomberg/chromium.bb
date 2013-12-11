// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/pin_hash.h"

#include "base/base64.h"
#include "base/logging.h"
#include "remoting/protocol/authentication_method.h"
#include "remoting/protocol/me2me_host_authenticator_factory.h"

namespace remoting {

std::string MakeHostPinHash(const std::string& host_id,
                            const std::string& pin) {
  std::string hash = protocol::AuthenticationMethod::ApplyHashFunction(
      protocol::AuthenticationMethod::HMAC_SHA256, host_id, pin);
  std::string hash_base64;
  base::Base64Encode(hash, &hash_base64);
  return "hmac:" + hash_base64;
}

bool VerifyHostPinHash(const std::string& hash,
                       const std::string& host_id,
                       const std::string& pin) {
  remoting::protocol::SharedSecretHash hash_parsed;
  if (!hash_parsed.Parse(hash)) {
    LOG(FATAL) << "Invalid hash.";
    return false;
  }
  std::string hash_calculated =
      remoting::protocol::AuthenticationMethod::ApplyHashFunction(
          hash_parsed.hash_function, host_id, pin);
  return hash_calculated == hash_parsed.value;
}

}  // namespace remoting
