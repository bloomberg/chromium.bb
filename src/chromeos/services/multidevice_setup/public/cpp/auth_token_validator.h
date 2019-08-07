// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_AUTH_TOKEN_VALIDATOR_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_AUTH_TOKEN_VALIDATOR_H_

#include <string>

#include "base/macros.h"

namespace chromeos {

namespace multidevice_setup {

// Validates a given auth token.
class AuthTokenValidator {
 public:
  AuthTokenValidator() = default;
  virtual ~AuthTokenValidator() = default;

  virtual bool IsAuthTokenValid(const std::string& auth_token) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AuthTokenValidator);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_AUTH_TOKEN_VALIDATOR_H_
