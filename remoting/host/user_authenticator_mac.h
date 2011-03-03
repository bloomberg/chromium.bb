// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_USER_AUTHENTICATOR_MAC_H_
#define REMOTING_HOST_USER_AUTHENTICATOR_MAC_H_

#include <string>

#include "base/basictypes.h"
#include "remoting/host/user_authenticator.h"

namespace remoting {

class UserAuthenticatorMac : public UserAuthenticator {
 public:
  UserAuthenticatorMac();
  virtual ~UserAuthenticatorMac();
  virtual bool Authenticate(const std::string& username,
                            const std::string& password);

 private:
  DISALLOW_COPY_AND_ASSIGN(UserAuthenticatorMac);
};

}  // namespace remoting

#endif  // REMOTING_HOST_USER_AUTHENTICATOR_MAC_H_
