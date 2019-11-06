// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/virtual_fido_discovery_factory.h"

#include <utility>
#include <vector>

#include "base/stl_util.h"
#include "content/browser/webauth/authenticator_type_converters.h"
#include "content/browser/webauth/virtual_authenticator.h"
#include "content/browser/webauth/virtual_discovery.h"
#include "device/fido/fido_discovery_base.h"
#include "device/fido/virtual_ctap2_device.h"

namespace content {

namespace {

blink::test::mojom::VirtualAuthenticatorPtr GetMojoPtrToVirtualAuthenticator(
    VirtualAuthenticator* authenticator) {
  blink::test::mojom::VirtualAuthenticatorPtr mojo_authenticator_ptr;
  authenticator->AddBinding(mojo::MakeRequest(&mojo_authenticator_ptr));
  return mojo_authenticator_ptr;
}

}  // namespace

VirtualFidoDiscoveryFactory::VirtualFidoDiscoveryFactory()
    : virtual_device_state_(new device::VirtualFidoDevice::State) {}

VirtualFidoDiscoveryFactory::~VirtualFidoDiscoveryFactory() = default;

void VirtualFidoDiscoveryFactory::AddBinding(
    blink::test::mojom::VirtualAuthenticatorManagerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void VirtualFidoDiscoveryFactory::OnDiscoveryDestroyed(
    VirtualFidoDiscovery* discovery) {
  if (base::ContainsKey(discoveries_, discovery))
    discoveries_.erase(discovery);
}

std::unique_ptr<::device::FidoDiscoveryBase>
VirtualFidoDiscoveryFactory::Create(device::FidoTransportProtocol transport,
                                    ::service_manager::Connector* connector) {
  auto discovery = std::make_unique<VirtualFidoDiscovery>(transport);

  if (bindings_.empty()) {
    // If no bindings are active then create a virtual device. This is useful
    // for web-platform tests which assume that they can make webauthn calls,
    // but which don't implement the Chromium-specific mock Mojo interfaces.
    device::VirtualCtap2Device::Config default_config;
    auto device = std::make_unique<device::VirtualCtap2Device>(
        virtual_device_state_, default_config);
    discovery->AddVirtualDevice(std::move(device));
  } else {
    for (auto& authenticator : authenticators_) {
      if (discovery->transport() != authenticator.second->transport())
        continue;
      discovery->AddVirtualDevice(authenticator.second->ConstructDevice());
    }
  }

  discoveries_.insert(discovery.get());
  return discovery;
}

std::unique_ptr<::device::FidoDiscoveryBase>
VirtualFidoDiscoveryFactory::CreateCable(
    std::vector<device::CableDiscoveryData> cable_data) {
  return Create(device::FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy,
                nullptr);
}

void VirtualFidoDiscoveryFactory::CreateAuthenticator(
    blink::test::mojom::VirtualAuthenticatorOptionsPtr options,
    CreateAuthenticatorCallback callback) {
  auto authenticator = std::make_unique<VirtualAuthenticator>(
      mojo::ConvertTo<::device::ProtocolVersion>(options->protocol),
      mojo::ConvertTo<::device::FidoTransportProtocol>(options->transport),
      mojo::ConvertTo<::device::AuthenticatorAttachment>(options->attachment),
      options->has_resident_key, options->has_user_verification);
  auto* authenticator_ptr = authenticator.get();
  authenticators_.emplace(authenticator_ptr->unique_id(),
                          std::move(authenticator));

  for (auto* discovery : discoveries_) {
    if (discovery->transport() != authenticator_ptr->transport())
      continue;
    discovery->AddVirtualDevice(authenticator_ptr->ConstructDevice());
  }

  std::move(callback).Run(GetMojoPtrToVirtualAuthenticator(authenticator_ptr));
}

void VirtualFidoDiscoveryFactory::GetAuthenticators(
    GetAuthenticatorsCallback callback) {
  std::vector<blink::test::mojom::VirtualAuthenticatorPtrInfo>
      mojo_authenticators;
  for (auto& authenticator : authenticators_) {
    mojo_authenticators.push_back(
        GetMojoPtrToVirtualAuthenticator(authenticator.second.get())
            .PassInterface());
  }

  std::move(callback).Run(std::move(mojo_authenticators));
}

void VirtualFidoDiscoveryFactory::RemoveAuthenticator(
    const std::string& id,
    RemoveAuthenticatorCallback callback) {
  const bool removed = authenticators_.erase(id);
  if (removed) {
    for (auto* discovery : discoveries_)
      discovery->RemoveVirtualDevice(id);
  }

  std::move(callback).Run(removed);
}

void VirtualFidoDiscoveryFactory::ClearAuthenticators(
    ClearAuthenticatorsCallback callback) {
  for (auto& authenticator : authenticators_) {
    for (auto* discovery : discoveries_) {
      discovery->RemoveVirtualDevice(authenticator.second->unique_id());
    }
  }
  authenticators_.clear();

  std::move(callback).Run();
}

}  // namespace content
