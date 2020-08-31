// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CROS_DISCOVERY_H_
#define DEVICE_FIDO_CROS_DISCOVERY_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "device/fido/cros/authenticator.h"
#include "device/fido/fido_discovery_base.h"

namespace device {

class COMPONENT_EXPORT(DEVICE_FIDO) FidoChromeOSDiscovery
    : public FidoDiscoveryBase {
 public:
  FidoChromeOSDiscovery();
  ~FidoChromeOSDiscovery() override;

  // FidoDiscoveryBase:
  void Start() override;

 private:
  void AddAuthenticator();

  std::unique_ptr<ChromeOSAuthenticator> authenticator_;
  base::WeakPtrFactory<FidoChromeOSDiscovery> weak_factory_;
};

}  // namespace device

#endif  // DEVICE_FIDO_CROS_DISCOVERY_H_
