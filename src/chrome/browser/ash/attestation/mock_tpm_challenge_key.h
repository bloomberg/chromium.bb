// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ATTESTATION_MOCK_TPM_CHALLENGE_KEY_H_
#define CHROME_BROWSER_ASH_ATTESTATION_MOCK_TPM_CHALLENGE_KEY_H_

#include <string>

#include "chrome/browser/ash/attestation/tpm_challenge_key.h"
#include "chromeos/dbus/constants/attestation_constants.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
namespace attestation {

class MockTpmChallengeKey : public TpmChallengeKey {
 public:
  MockTpmChallengeKey();
  ~MockTpmChallengeKey() override;

  void EnableFake();

  MOCK_METHOD(
      void,
      BuildResponse,
      (chromeos::attestation::AttestationKeyType key_type,
       Profile* profile,
       TpmChallengeKeyCallback callback,
       const std::string& challenge,
       bool register_key,
       const std::string& key_name_for_spkac,
       const absl::optional<::attestation::DeviceTrustSignals>& signals),
      (override));

  void FakeBuildResponseSuccess(TpmChallengeKeyCallback callback);
};

}  // namespace attestation
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ATTESTATION_MOCK_TPM_CHALLENGE_KEY_H_
