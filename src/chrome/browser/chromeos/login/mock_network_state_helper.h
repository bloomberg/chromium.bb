// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_NETWORK_STATE_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_NETWORK_STATE_HELPER_H_

#include "chrome/browser/chromeos/login/helper.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

namespace login {

class MockNetworkStateHelper : public NetworkStateHelper {
 public:
  MockNetworkStateHelper();
  ~MockNetworkStateHelper() override;
  MOCK_CONST_METHOD0(GetCurrentNetworkName, base::string16(void));
  MOCK_CONST_METHOD0(IsConnected, bool(void));
  MOCK_CONST_METHOD0(IsConnecting, bool(void));
};

}  // namespace login

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_NETWORK_STATE_HELPER_H_
