// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PROXIMITY_AUTH_MOCK_PROXIMITY_AUTH_CLIENT_H_
#define CHROMEOS_COMPONENTS_PROXIMITY_AUTH_MOCK_PROXIMITY_AUTH_CLIENT_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "chromeos/components/proximity_auth/proximity_auth_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace proximity_auth {

// Mock implementation of ProximityAuthClient.
class MockProximityAuthClient : public ProximityAuthClient {
 public:
  MockProximityAuthClient();
  ~MockProximityAuthClient() override;

  // ProximityAuthClient:
  MOCK_METHOD1(UpdateScreenlockState,
               void(proximity_auth::ScreenlockState state));
  MOCK_METHOD1(FinalizeUnlock, void(bool success));
  MOCK_METHOD1(FinalizeSignin, void(const std::string& secret));
  MOCK_METHOD4(
      GetChallengeForUserAndDevice,
      void(const std::string& user_id,
           const std::string& remote_public_key,
           const std::string& channel_binding_data,
           base::Callback<void(const std::string& challenge)> callback));
  MOCK_CONST_METHOD0(GetAuthenticatedUsername, std::string(void));
  MOCK_METHOD0(GetPrefManager, ProximityAuthPrefManager*(void));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockProximityAuthClient);
};

}  // namespace proximity_auth

#endif  // CHROMEOS_COMPONENTS_PROXIMITY_AUTH_MOCK_PROXIMITY_AUTH_CLIENT_H_
