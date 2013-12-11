// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/auth_util.h"

#include "base/base64.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "crypto/hmac.h"
#include "crypto/sha2.h"
#include "net/base/net_errors.h"
#include "net/socket/ssl_socket.h"

namespace remoting {
namespace protocol {

const char kClientAuthSslExporterLabel[] =
    "EXPORTER-remoting-channel-auth-client";
const char kHostAuthSslExporterLabel[] =
    "EXPORTER-remoting-channel-auth-host";

const char kSslFakeHostName[] = "chromoting";

std::string GenerateSupportAuthToken(const std::string& jid,
                                     const std::string& access_code) {
  std::string sha256 = crypto::SHA256HashString(jid + " " + access_code);
  std::string sha256_base64;
  base::Base64Encode(sha256, &sha256_base64);
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
std::string GetAuthBytes(net::SSLSocket* socket,
                         const base::StringPiece& label,
                         const base::StringPiece& shared_secret) {
  // Get keying material from SSL.
  unsigned char key_material[kAuthDigestLength];
  int export_result = socket->ExportKeyingMaterial(
      label, false, "", key_material, kAuthDigestLength);
  if (export_result != net::OK) {
    LOG(ERROR) << "Error fetching keying material: " << export_result;
    return std::string();
  }

  // Generate auth digest based on the keying material and shared secret.
  crypto::HMAC response(crypto::HMAC::SHA256);
  if (!response.Init(key_material, kAuthDigestLength)) {
    NOTREACHED() << "HMAC::Init failed";
    return std::string();
  }
  unsigned char out_bytes[kAuthDigestLength];
  if (!response.Sign(shared_secret, out_bytes, kAuthDigestLength)) {
    NOTREACHED() << "HMAC::Sign failed";
    return std::string();
  }

  return std::string(out_bytes, out_bytes + kAuthDigestLength);
}

}  // namespace protocol
}  // namespace remoting
