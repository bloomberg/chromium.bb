// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_COMMON_MOCK_ATTESTATION_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_COMMON_MOCK_ATTESTATION_SERVICE_H_

#include <string>

#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/attestation_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/signals_type.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace enterprise_connectors {
namespace test {

// Interface for classes in charge of building challenge-responses to enable
// handshake between Chrome, an IdP and Verified Access.
class MockAttestationService : public AttestationService {
 public:
  MockAttestationService();
  ~MockAttestationService() override;

  MOCK_METHOD(void,
              BuildChallengeResponseForVAChallenge,
              (const std::string&,
               std::unique_ptr<SignalsType>,
               AttestationCallback),
              (override));
};

}  // namespace test
}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_COMMON_MOCK_ATTESTATION_SERVICE_H_
