// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_secret.h"

#include <vector>

#include "base/logging.h"
#include "base/rand_util.h"
#include "base/string_number_conversions.h"

namespace remoting {

namespace {

// 5 digits means 100K possible host secrets with uniform distribution, which
// should be enough for short-term passwords, given that we rate-limit guesses
// in the cloud and expire access codes after a small number of attempts.
const int kMaxHostSecret = 100000;

// Generates cryptographically strong random number in the range [0, max).
int CryptoRandomInt(int max) {
  uint64 random_int32;
  base::RandBytes(&random_int32, sizeof(random_int32));
  return random_int32 % max;
}

}  // namespace

std::string GenerateSupportHostSecret() {
  return base::IntToString(CryptoRandomInt(kMaxHostSecret));
}

}  // namespace remoting
