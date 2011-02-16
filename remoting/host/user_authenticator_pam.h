// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_USER_AUTHENTICATOR_PAM_H_
#define REMOTING_HOST_USER_AUTHENTICATOR_PAM_H_

#include <string>

#include "base/basictypes.h"
#include "remoting/host/user_authenticator.h"

struct pam_message;
struct pam_response;

namespace remoting {

// Class to perform a single PAM user authentication.
//
// TODO(lambroslambrou): As pam_authenticate() can be blocking, this needs to
// expose an asynchronous API, with pam_authenticate() called in a background
// thread.
class UserAuthenticatorPam : public UserAuthenticator {
 public:
  UserAuthenticatorPam();
  virtual ~UserAuthenticatorPam();
  virtual bool Authenticate(const std::string& username,
                            const std::string& password);

 private:
  // Conversation function passed to PAM as a callback.
  static int ConvFunction(int num_msg,
                          const pam_message** msg,
                          pam_response** resp,
                          void* appdata_ptr);

  // Store these for the PAM conversation function.
  std::string username_;
  std::string password_;

  DISALLOW_COPY_AND_ASSIGN(UserAuthenticatorPam);
};

}  // namespace remoting

#endif  // REMOTING_HOST_USER_AUTHENTICATOR_PAM_H_
