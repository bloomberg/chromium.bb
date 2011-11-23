// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/auth_util.h"

#include "base/base64.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "crypto/hmac.h"
#include "crypto/sha2.h"

namespace remoting {
namespace protocol {

const char kClientAuthSslExporterLabel[] =
    "EXPORTER-remoting-channel-auth-client";

const char kSslFakeHostName[] = "chromoting";

std::string GenerateSupportAuthToken(const std::string& jid,
                                     const std::string& access_code) {
  std::string sha256 = crypto::SHA256HashString(jid + " " + access_code);
  std::string sha256_base64;
  if (!base::Base64Encode(sha256, &sha256_base64)) {
    LOG(FATAL) << "Failed to encode auth token";
  }
  return sha256_base64;
}

bool VerifySupportAuthToken(const std::string& jid,
                            const std::string& access_code,
                            const std::string& auth_token) {
  std::string expected_token =
      GenerateSupportAuthToken(jid, access_code);
  return expected_token == auth_token;
}

// static
bool GetAuthBytes(const std::string& shared_secret,
                  const std::string& key_material,
                  std::string* auth_bytes) {
  // Generate auth digest based on the keying material and shared secret.
  crypto::HMAC response(crypto::HMAC::SHA256);
  if (!response.Init(key_material)) {
    NOTREACHED() << "HMAC::Init failed";
    return false;
  }
  unsigned char out_bytes[kAuthDigestLength];
  if (!response.Sign(shared_secret, out_bytes, kAuthDigestLength)) {
    NOTREACHED() << "HMAC::Sign failed";
    return false;
  }

  auth_bytes->assign(out_bytes, out_bytes + kAuthDigestLength);
  return true;
}

}  // namespace protocol
}  // namespace remoting
