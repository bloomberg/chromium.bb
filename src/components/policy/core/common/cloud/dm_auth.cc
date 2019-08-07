// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/dm_auth.h"

namespace policy {

DMAuth::DMAuth() = default;
DMAuth::~DMAuth() = default;

// static
std::unique_ptr<DMAuth> DMAuth::FromDMToken(const std::string& dm_token) {
  std::unique_ptr<DMAuth> result = std::make_unique<DMAuth>();
  result->dm_token_ = dm_token;
  return result;
}

// static
std::unique_ptr<DMAuth> DMAuth::FromGaiaToken(const std::string& gaia_token) {
  std::unique_ptr<DMAuth> result = std::make_unique<DMAuth>();
  result->gaia_token_ = gaia_token;
  return result;
}

// static
std::unique_ptr<DMAuth> DMAuth::FromOAuthToken(const std::string& oauth_token) {
  std::unique_ptr<DMAuth> result = std::make_unique<DMAuth>();
  result->oauth_token_ = oauth_token;
  return result;
}

// static
std::unique_ptr<DMAuth> DMAuth::FromEnrollmentToken(
    const std::string& enrollment_token) {
  std::unique_ptr<DMAuth> result = std::make_unique<DMAuth>();
  result->enrollment_token_ = enrollment_token;
  return result;
}
// static
std::unique_ptr<DMAuth> DMAuth::NoAuth() {
  return std::make_unique<DMAuth>();
}

std::unique_ptr<DMAuth> DMAuth::Clone() const {
  std::unique_ptr<DMAuth> result = std::make_unique<DMAuth>();
  *result = *this;
  return result;
}

bool DMAuth::empty() const {
  return gaia_token_.empty() && dm_token_.empty() &&
         enrollment_token_.empty() && oauth_token_.empty();
}

bool DMAuth::Equals(const DMAuth& other) const {
  return gaia_token_ == other.gaia_token_ && dm_token_ == other.dm_token_ &&
         enrollment_token_ == other.enrollment_token_ &&
         oauth_token_ == other.oauth_token_;
}

}  // namespace policy
