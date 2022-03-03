// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_SERVICE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/signals_type.h"
#include "components/keyed_service/core/keyed_service.h"

class GURL;

namespace enterprise_connectors {

class AttestationService;
class DeviceTrustConnectorService;
class SignalsService;

// Main service used to drive device trust connector scenarios. It is currently
// used to generate a response for a crypto challenge received from Verified
// Access during an attestation flow.
class DeviceTrustService : public KeyedService {
 public:
  using AttestationCallback = base::OnceCallback<void(const std::string&)>;

  DeviceTrustService(std::unique_ptr<AttestationService> attestation_service,
                     std::unique_ptr<SignalsService> signals_service,
                     DeviceTrustConnectorService* connector);

  DeviceTrustService(const DeviceTrustService&) = delete;
  DeviceTrustService& operator=(const DeviceTrustService&) = delete;

  ~DeviceTrustService() override;

  // Check if DeviceTrustService is enabled.  This method may be called from
  // any task sequence.
  virtual bool IsEnabled() const;

  // Starts flow that actually builds a response.
  virtual void BuildChallengeResponse(const std::string& challenge,
                                      AttestationCallback callback);

  // Returns whether the Device Trust connector watches navigations to the given
  // `url` or not.
  virtual bool Watches(const GURL& url) const;

  // Collects device trust signals and returns them via `callback`.
  void GetSignals(
      base::OnceCallback<void(std::unique_ptr<SignalsType>)> callback);

 protected:
  // Default constructor that can be used by mocks to bypass initialization.
  DeviceTrustService();

 private:
  void OnSignalsCollected(const std::string& challenge,
                          AttestationCallback callback,
                          std::unique_ptr<SignalsType> signals);

  std::unique_ptr<AttestationService> attestation_service_;
  std::unique_ptr<SignalsService> signals_service_;
  const raw_ptr<DeviceTrustConnectorService> connector_{nullptr};

  base::WeakPtrFactory<DeviceTrustService> weak_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_SERVICE_H_
