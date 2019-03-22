// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FIDO_DISCOVERY_FACTORY_H_
#define DEVICE_FIDO_FIDO_DISCOVERY_FACTORY_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "build/build_config.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/fido_device_discovery.h"
#include "device/fido/fido_discovery_base.h"
#include "device/fido/fido_transport_protocol.h"

namespace service_manager {
class Connector;
}

namespace device {

namespace internal {
class ScopedFidoDiscoveryFactory;
}

// FidoDiscoveryFactory offers methods to construct instances of
// FidoDiscoveryBase for a given |transport| protocol.
class COMPONENT_EXPORT(DEVICE_FIDO) FidoDiscoveryFactory {
 public:
  // Instantiates a FidoDiscoveryBase for all protocols except caBLE and
  // internal/platform.
  //
  // FidoTransportProtocol::kUsbHumanInterfaceDevice requires specifying a
  // valid |connector| on Desktop, and is not valid on Android.
  static std::unique_ptr<FidoDiscoveryBase> Create(
      FidoTransportProtocol transport,
      ::service_manager::Connector* connector);
  // Instantiates a FidoDiscovery for caBLE.
  static std::unique_ptr<FidoDiscoveryBase> CreateCable(
      std::vector<CableDiscoveryData> cable_data);
#if defined(OS_WIN)
  // Instantiates a FidoDiscovery for the native Windows WebAuthn
  // API where available. Returns nullptr otherwise.
  static std::unique_ptr<FidoDiscoveryBase>
  MaybeCreateWinWebAuthnApiDiscovery();
#endif  // defined(OS_WIN)

 private:
  friend class internal::ScopedFidoDiscoveryFactory;

  // Factory function can be overridden by tests to construct fakes.
  using FactoryFuncPtr = decltype(&Create);
  using CableFactoryFuncPtr = decltype(&CreateCable);
  static FactoryFuncPtr g_factory_func_;
  static CableFactoryFuncPtr g_cable_factory_func_;
};

namespace internal {
// Base class for a scoped override of FidoDiscoveryFactory::Create, used in
// unit tests, layout tests, and when running with the Web Authn Testing API
// enabled.
//
// While there is a subclass instance in scope, calls to the factory method will
// be hijacked such that the derived class's CreateFidoDiscovery method will be
// invoked instead.
class COMPONENT_EXPORT(DEVICE_FIDO) ScopedFidoDiscoveryFactory {
 public:
  // There should be at most one instance of any subclass in scope at a time.
  ScopedFidoDiscoveryFactory();
  virtual ~ScopedFidoDiscoveryFactory();

  const std::vector<CableDiscoveryData>& last_cable_data() const {
    return last_cable_data_;
  }

 protected:
  void set_last_cable_data(std::vector<CableDiscoveryData> cable_data) {
    last_cable_data_ = std::move(cable_data);
  }

  virtual std::unique_ptr<FidoDiscoveryBase> CreateFidoDiscovery(
      FidoTransportProtocol transport,
      ::service_manager::Connector* connector) = 0;

 private:
  static std::unique_ptr<FidoDiscoveryBase>
  ForwardCreateFidoDiscoveryToCurrentFactory(
      FidoTransportProtocol transport,
      ::service_manager::Connector* connector);

  static std::unique_ptr<FidoDiscoveryBase>
  ForwardCreateCableDiscoveryToCurrentFactory(
      std::vector<CableDiscoveryData> cable_data);

  static ScopedFidoDiscoveryFactory* g_current_factory;

  FidoDiscoveryFactory::FactoryFuncPtr original_factory_func_;
  FidoDiscoveryFactory::CableFactoryFuncPtr original_cable_factory_func_;
  std::vector<CableDiscoveryData> last_cable_data_;

  DISALLOW_COPY_AND_ASSIGN(ScopedFidoDiscoveryFactory);
};

}  // namespace internal
}  // namespace device

#endif  // DEVICE_FIDO_FIDO_DISCOVERY_FACTORY_H_
