// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/support_access_verifier.h"

#include <stdlib.h>

#include "base/logging.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "remoting/host/host_config.h"
#include "remoting/protocol/auth_token_utils.h"

namespace remoting {

namespace {
// 5 characters long from 34-letter alphabet gives 4.5M possible
// access codes with uniform distribution, which should be enough
// for short-term passwords.
const int kAccessCodeLength = 5;

// The following set includes 10 digits and Latin alphabet except I
// and O. I and O are not used to avoid confusion with 1 and 0.
const char kAccessCodeAlphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ0123456789";

// Generates cryptographically strong random number in the range [0, max).
int CryptoRandomInt(int max) {
  uint32 random_int32;
  base::RandBytes(&random_int32, sizeof(random_int32));
  return random_int32 % max;
}

std::string GenerateRandomAccessCode() {
  std::vector<char> result;
  int alphabet_size = strlen(kAccessCodeAlphabet);
  result.resize(kAccessCodeLength);
  for (int i = 0; i < kAccessCodeLength; ++i) {
    result[i] = kAccessCodeAlphabet[CryptoRandomInt(alphabet_size)];
  }
  return std::string(result.begin(), result.end());
}

}  // namespace

SupportAccessVerifier::SupportAccessVerifier()
    : initialized_(false) {
}

SupportAccessVerifier::~SupportAccessVerifier() { }

bool SupportAccessVerifier::Init() {
  access_code_ = GenerateRandomAccessCode();
  initialized_ = true;
  return true;
}

bool SupportAccessVerifier::VerifyPermissions(
    const std::string& client_jid,
    const std::string& encoded_access_token) {
  CHECK(initialized_);
  return protocol::VerifySupportAuthToken(
      client_jid, access_code_, encoded_access_token);
}

}  // namespace remoting
