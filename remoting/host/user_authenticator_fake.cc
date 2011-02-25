// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/user_authenticator_fake.h"

#include <string>

namespace remoting {

UserAuthenticatorFake::UserAuthenticatorFake() {}
UserAuthenticatorFake::~UserAuthenticatorFake() {}

bool UserAuthenticatorFake::Authenticate(const std::string& username,
                                         const std::string& password) {
  return true;
}

}  // namespace remoting
