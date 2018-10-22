// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/oobe_configuration_client.h"

#include <memory>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

// The OobeConfigurationClient used in production.

class OobeConfigurationClientImpl : public OobeConfigurationClient {
 public:
  OobeConfigurationClientImpl() : weak_ptr_factory_(this) {}

  ~OobeConfigurationClientImpl() override = default;

  // OobeConfigurationClient override:
  void CheckForOobeConfiguration(ConfigurationCallback callback) override {
    // TODO (antrim): do a method call once https://crbug.com/869209 is fixed.
    OnData(std::move(callback), nullptr);
  }

 protected:
  void Init(dbus::Bus* bus) override {
    // TODO (antrim): get proxy object once https://crbug.com/869209 is fixed.
  }

 private:
  void OnData(ConfigurationCallback callback, dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(false, std::string());
      return;
    }

    // TODO (antrim): read response once https://crbug.com/869209 is fixed.
    NOTREACHED();
  }

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<OobeConfigurationClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OobeConfigurationClientImpl);
};

// static
std::unique_ptr<OobeConfigurationClient> OobeConfigurationClient::Create() {
  return std::make_unique<OobeConfigurationClientImpl>();
}

}  // namespace chromeos
