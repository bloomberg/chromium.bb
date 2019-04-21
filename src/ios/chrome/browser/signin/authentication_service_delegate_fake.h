// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_AUTHENTICATION_SERVICE_DELEGATE_FAKE_H_
#define IOS_CHROME_BROWSER_SIGNIN_AUTHENTICATION_SERVICE_DELEGATE_FAKE_H_

#import "ios/chrome/browser/signin/authentication_service_delegate.h"

// Fake AuthenticationServiceDelegate used by AuthenticationServiceFake.
class AuthenticationServiceDelegateFake : public AuthenticationServiceDelegate {
 public:
  AuthenticationServiceDelegateFake();
  ~AuthenticationServiceDelegateFake() override;

  // AuthenticationServiceDelegate implementation.
  void ClearBrowsingData(ProceduralBlock completion) override;

 private:
  DISALLOW_ASSIGN(AuthenticationServiceDelegateFake);
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_AUTHENTICATION_SERVICE_DELEGATE_FAKE_H_
