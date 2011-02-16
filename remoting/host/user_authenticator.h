// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_USER_AUTHENTICATOR_H_
#define REMOTING_HOST_USER_AUTHENTICATOR_H_

#include <string>

namespace remoting {

// Interface for authenticating users for access to remote desktop session.
// Implementation is platform-specific.
// Implementations may assume each instance of this class handles only a
// single Authenticate request.
// TODO(lambroslambrou): Decide whether this needs an asychronous interface
// (for example AuthenticateStart()..AuthenticateEndCallback()), or whether the
// multi-threading policy could be handled by the caller.
class UserAuthenticator {
 public:
  virtual ~UserAuthenticator() {}

  // Authenticate a user, returning true if the username/password are valid.
  virtual bool Authenticate(const std::string& username,
                            const std::string& password) = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_USER_AUTHENTICATOR_H_
