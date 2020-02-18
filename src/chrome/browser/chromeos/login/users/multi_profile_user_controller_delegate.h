// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USERS_MULTI_PROFILE_USER_CONTROLLER_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USERS_MULTI_PROFILE_USER_CONTROLLER_DELEGATE_H_

namespace chromeos {

class MultiProfileUserControllerDelegate {
 public:
  // Invoked when the first user that is not allowed in the session is detected.
  virtual void OnUserNotAllowed(const std::string& user_email) = 0;

 protected:
  virtual ~MultiProfileUserControllerDelegate() {}
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USERS_MULTI_PROFILE_USER_CONTROLLER_DELEGATE_H_
