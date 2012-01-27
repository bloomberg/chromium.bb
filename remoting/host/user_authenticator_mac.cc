// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/user_authenticator.h"

#include <Security/Security.h>

#include <string>

#include "base/basictypes.h"
#include "base/mac/mac_logging.h"

namespace remoting {

namespace {

class UserAuthenticatorMac : public UserAuthenticator {
 public:
  UserAuthenticatorMac() {}
  virtual ~UserAuthenticatorMac() {}
  virtual bool Authenticate(const std::string& username,
                            const std::string& password);

 private:
  DISALLOW_COPY_AND_ASSIGN(UserAuthenticatorMac);
};

const char kAuthorizationRightName[] = "system.login.tty";

bool UserAuthenticatorMac::Authenticate(const std::string& username,
                                        const std::string& password) {
  // The authorization right being requested.  This particular right allows
  // testing of a username/password, as if the user were logging on to the
  // system locally.
  AuthorizationItem right;
  right.name = kAuthorizationRightName;
  right.valueLength = 0;
  right.value = NULL;
  right.flags = 0;
  AuthorizationRights rights;
  rights.count = 1;
  rights.items = &right;

  // Passing the username/password as an "environment" parameter causes these
  // to be submitted to the Security Framework, instead of the interactive
  // password prompt appearing on the host system.  Valid on OS X 10.4 and
  // later versions.
  AuthorizationItem environment_items[2];
  environment_items[0].name = kAuthorizationEnvironmentUsername;
  environment_items[0].valueLength = username.size();
  environment_items[0].value = const_cast<char*>(username.data());
  environment_items[0].flags = 0;
  environment_items[1].name = kAuthorizationEnvironmentPassword;
  environment_items[1].valueLength = password.size();
  environment_items[1].value = const_cast<char*>(password.data());
  environment_items[1].flags = 0;
  AuthorizationEnvironment environment;
  environment.count = 2;
  environment.items = environment_items;

  OSStatus status = AuthorizationCreate(&rights, &environment,
                                        kAuthorizationFlagExtendRights,
                                        NULL);
  switch (status) {
    case errAuthorizationSuccess:
      return true;

    case errAuthorizationDenied:
      return false;

    default:
      OSSTATUS_LOG(ERROR, status) << "AuthorizationCreate";
      return false;
  }
}

}  // namespace

// static
UserAuthenticator* UserAuthenticator::Create() {
  return new UserAuthenticatorMac();
}

}  // namespace remoting
