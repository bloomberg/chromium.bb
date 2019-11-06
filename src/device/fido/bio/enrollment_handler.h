// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_BIO_ENROLLMENT_HANDLER_H_
#define DEVICE_FIDO_BIO_ENROLLMENT_HANDLER_H_

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "device/fido/bio/enrollment.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_discovery_factory.h"
#include "device/fido/fido_request_handler_base.h"

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace device {

class COMPONENT_EXPORT(DEVICE_FIDO) BioEnrollmentHandler
    : public FidoRequestHandlerBase {
 public:
  using GetInfoCallback =
      base::OnceCallback<void(CtapDeviceResponseCode,
                              base::Optional<BioEnrollmentResponse>)>;

  BioEnrollmentHandler(
      service_manager::Connector* connector,
      FidoDiscoveryFactory* factory,
      const base::flat_set<FidoTransportProtocol>& supported_transports,
      base::OnceClosure ready_callback);
  ~BioEnrollmentHandler() override;

  void GetModality(GetInfoCallback callback);
  void GetSensorInfo(GetInfoCallback callback);

 private:
  // FidoRequestHandlerBase:
  void DispatchRequest(FidoAuthenticator*) override;
  void AuthenticatorRemoved(FidoDiscoveryBase*, FidoAuthenticator*) override;

  void OnTouch(FidoAuthenticator* authenticator);
  void OnGetInfo(CtapDeviceResponseCode, base::Optional<BioEnrollmentResponse>);

  SEQUENCE_CHECKER(sequence_checker);

  FidoAuthenticator* authenticator_ = nullptr;
  base::OnceClosure ready_callback_;
  GetInfoCallback callback_;
  base::WeakPtrFactory<BioEnrollmentHandler> weak_factory_;

  BioEnrollmentHandler(const BioEnrollmentHandler&) = delete;
  BioEnrollmentHandler(BioEnrollmentHandler&&) = delete;
};

}  // namespace device

#endif  // DEVICE_FIDO_BIO_ENROLLMENT_HANDLER_H_
