// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/support_access_verifier.h"

#include <stdlib.h>
#include <vector>

#include "base/logging.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "remoting/host/host_config.h"
#include "remoting/protocol/auth_token_utils.h"

namespace remoting {

namespace {
// 5 characters long from 34-letter alphabet gives 4.5M possible
// host secrets with uniform distribution, which should be enough
// for short-term passwords.
const int kHostSecretLength = 5;

// The following set includes 10 digits and Latin alphabet except I
// and O. I and O are not used to avoid confusion with 1 and 0.
const char kHostSecretAlphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ0123456789";

// Generates cryptographically strong random number in the range [0, max).
int CryptoRandomInt(int max) {
  uint32 random_int32;
  base::RandBytes(&random_int32, sizeof(random_int32));
  return random_int32 % max;
}

std::string GenerateRandomHostSecret() {
  std::vector<char> result;
  int alphabet_size = strlen(kHostSecretAlphabet);
  result.resize(kHostSecretLength);
  for (int i = 0; i < kHostSecretLength; ++i) {
    result[i] = kHostSecretAlphabet[CryptoRandomInt(alphabet_size)];
  }
  return std::string(result.begin(), result.end());
}

}  // namespace

SupportAccessVerifier::SupportAccessVerifier()
    : initialized_(false) {
}

SupportAccessVerifier::~SupportAccessVerifier() { }

bool SupportAccessVerifier::Init() {
  host_secret_ = GenerateRandomHostSecret();
  initialized_ = true;
  return true;
}

bool SupportAccessVerifier::VerifyPermissions(
    const std::string& client_jid,
    const std::string& encoded_access_token) {
  CHECK(initialized_);
  if (support_id_.empty())
    return false;
  std::string access_code = support_id_ + "-" + host_secret_;
  return protocol::VerifySupportAuthToken(
      client_jid, access_code, encoded_access_token);
}

void SupportAccessVerifier::OnMe2MomHostRegistered(
    bool successful, const std::string& support_id) {
  if (successful) {
    support_id_ = support_id;
  } else {
    LOG(ERROR) << "Failed to register support host";
  }
}

}  // namespace remoting
