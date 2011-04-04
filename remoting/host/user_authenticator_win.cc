// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/user_authenticator.h"

#include <string>

#include "base/basictypes.h"
#include "base/logging.h"

namespace remoting {

namespace {

class UserAuthenticatorWin : public UserAuthenticator {
 public:
  UserAuthenticatorWin() {}
  virtual ~UserAuthenticatorWin() {}

  virtual bool Authenticate(const std::string& username,
                            const std::string& password);

 private:
  DISALLOW_COPY_AND_ASSIGN(UserAuthenticatorWin);
};

bool UserAuthenticatorWin::Authenticate(const std::string& username,
                                        const std::string& password) {
  NOTIMPLEMENTED();
  return true;
}

}  // namespace

// static
UserAuthenticator* UserAuthenticator::Create() {
  return new UserAuthenticatorWin();
}

}  // namespace remoting
