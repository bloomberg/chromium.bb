// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_AUTHENTICATION_SERVICE_OBSERVER_H_
#define IOS_CHROME_BROWSER_SIGNIN_AUTHENTICATION_SERVICE_OBSERVER_H_

#include "base/observer_list_types.h"

// Observer handling events related to the AuthenticationService.
class AuthenticationServiceObserver : public base::CheckedObserver {
 public:
  AuthenticationServiceObserver() = default;

  // Called when the primary account is filtered out due to an
  // enterprise restriction.
  virtual void OnPrimaryAccountRestricted() {}
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_AUTHENTICATION_SERVICE_OBSERVER_H_
