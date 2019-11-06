// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_CELLULAR_SETUP_PUBLIC_CPP_FAKE_CARRIER_PORTAL_HANDLER_H_
#define CHROMEOS_SERVICES_CELLULAR_SETUP_PUBLIC_CPP_FAKE_CARRIER_PORTAL_HANDLER_H_

#include <vector>

#include "base/macros.h"
#include "chromeos/services/cellular_setup/public/mojom/cellular_setup.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace chromeos {

namespace cellular_setup {

// Fake mojom::CarrierPortalHandler implementation.
class FakeCarrierPortalHandler : public mojom::CarrierPortalHandler {
 public:
  FakeCarrierPortalHandler();
  ~FakeCarrierPortalHandler() override;

  mojom::CarrierPortalHandlerPtr GenerateInterfacePtr();

  const std::vector<mojom::CarrierPortalStatus>& status_updates() const {
    return status_updates_;
  }

 private:
  // mojom::CarrierPortalHandler:
  void OnCarrierPortalStatusChange(
      mojom::CarrierPortalStatus carrier_portal_status) override;

  std::vector<mojom::CarrierPortalStatus> status_updates_;

  mojo::BindingSet<mojom::CarrierPortalHandler> bindings_;

  DISALLOW_COPY_AND_ASSIGN(FakeCarrierPortalHandler);
};

}  // namespace cellular_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_CELLULAR_SETUP_PUBLIC_CPP_FAKE_CARRIER_PORTAL_HANDLER_H_
