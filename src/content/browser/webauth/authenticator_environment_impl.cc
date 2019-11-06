// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/authenticator_environment_impl.h"

#include <utility>

#include "base/command_line.h"
#include "base/stl_util.h"
#include "content/browser/webauth/virtual_discovery.h"
#include "content/browser/webauth/virtual_fido_discovery_factory.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_switches.h"
#include "device/fido/fido_discovery_factory.h"

namespace content {

// static
AuthenticatorEnvironment* AuthenticatorEnvironment::GetInstance() {
  return AuthenticatorEnvironmentImpl::GetInstance();
}

// static
AuthenticatorEnvironmentImpl* AuthenticatorEnvironmentImpl::GetInstance() {
  static base::NoDestructor<AuthenticatorEnvironmentImpl> environment;
  return environment.get();
}

AuthenticatorEnvironmentImpl::AuthenticatorEnvironmentImpl() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableWebAuthTestingAPI)) {
    discovery_factory_ = std::make_unique<VirtualFidoDiscoveryFactory>();
  } else {
    discovery_factory_ = std::make_unique<device::FidoDiscoveryFactory>();
  }
}

AuthenticatorEnvironmentImpl::~AuthenticatorEnvironmentImpl() = default;

device::FidoDiscoveryFactory* AuthenticatorEnvironmentImpl::GetFactory(
    RenderFrameHost* host) {
  auto* factory = GetVirtualFactoryFor(host);
  if (factory)
    return factory;
  return discovery_factory_.get();
}

device::FidoDiscoveryFactory* AuthenticatorEnvironmentImpl::GetFactory() {
  return discovery_factory_.get();
}

void AuthenticatorEnvironmentImpl::EnableVirtualAuthenticatorFor(
    RenderFrameHost* host) {
  // Do not create a new virtual authenticator if there is one already defined
  // for the |host|.
  if (base::ContainsKey(virtual_discovery_factories_, host))
    return;
  virtual_discovery_factories_[host] =
      std::make_unique<VirtualFidoDiscoveryFactory>();
}

void AuthenticatorEnvironmentImpl::DisableVirtualAuthenticatorFor(
    RenderFrameHost* host) {
  if (base::ContainsKey(virtual_discovery_factories_, host))
    virtual_discovery_factories_.erase(host);
}

void AuthenticatorEnvironmentImpl::AddVirtualAuthenticatorBinding(
    RenderFrameHost* host,
    blink::test::mojom::VirtualAuthenticatorManagerRequest request) {
  auto* factory = GetVirtualFactoryFor(host);
  DCHECK(factory);
  factory->AddBinding(std::move(request));
}

void AuthenticatorEnvironmentImpl::ReplaceDefaultDiscoveryFactoryForTesting(
    std::unique_ptr<device::FidoDiscoveryFactory> factory) {
  discovery_factory_ = std::move(factory);
}

void AuthenticatorEnvironmentImpl::OnDiscoveryDestroyed(
    VirtualFidoDiscovery* discovery) {
  for (auto& it : virtual_discovery_factories_) {
    it.second->OnDiscoveryDestroyed(discovery);
  }
}

VirtualFidoDiscoveryFactory* AuthenticatorEnvironmentImpl::GetVirtualFactoryFor(
    RenderFrameHost* host) {
  do {
    if (base::ContainsKey(virtual_discovery_factories_, host)) {
      return virtual_discovery_factories_[host].get();
    }
  } while ((host = host->GetParent()));
  return nullptr;
}

}  // namespace content
