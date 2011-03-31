// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_USER_AUTHENTICATOR_FAKE_H_
#define REMOTING_HOST_USER_AUTHENTICATOR_FAKE_H_

#include <string>

#include "base/basictypes.h"
#include "remoting/host/user_authenticator.h"

namespace remoting {

// A fake UserAuthenticator, which accepts all but one user/password pair.
class UserAuthenticatorFake : public UserAuthenticator {
 public:
  UserAuthenticatorFake();
  virtual ~UserAuthenticatorFake();

  virtual bool Authenticate(const std::string& username,
                            const std::string& password);

  // Get the user/password pair that a UserAuthenticatorFake rejects.
  static const char* fail_username();
  static const char* fail_password();

 private:
  DISALLOW_COPY_AND_ASSIGN(UserAuthenticatorFake);
};

}  // namespace remoting

#endif  // REMOTING_HOST_USER_AUTHENTICATOR_FAKE_H_
