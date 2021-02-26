// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/authenticator_environment_impl.h"

#include <algorithm>
#include <utility>

#include "base/command_line.h"
#include "base/stl_util.h"
#include "content/browser/webauth/virtual_authenticator.h"
#include "content/browser/webauth/virtual_discovery.h"
#include "content/browser/webauth/virtual_fido_discovery_factory.h"
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

AuthenticatorEnvironmentImpl::AuthenticatorEnvironmentImpl() = default;

AuthenticatorEnvironmentImpl::~AuthenticatorEnvironmentImpl() = default;

void AuthenticatorEnvironmentImpl::EnableVirtualAuthenticatorFor(
    FrameTreeNode* node) {
  // Do not create a new virtual authenticator if there is one already defined
  // for the |node|.
  if (base::Contains(virtual_authenticator_managers_, node))
    return;

  node->AddObserver(this);
  virtual_authenticator_managers_[node] =
      std::make_unique<VirtualAuthenticatorManagerImpl>();
}

void AuthenticatorEnvironmentImpl::DisableVirtualAuthenticatorFor(
    FrameTreeNode* node) {
  if (!base::Contains(virtual_authenticator_managers_, node))
    return;

  node->RemoveObserver(this);
  virtual_authenticator_managers_.erase(node);
}

bool AuthenticatorEnvironmentImpl::IsVirtualAuthenticatorEnabledFor(
    FrameTreeNode* node) {
  return MaybeGetVirtualAuthenticatorManager(node) != nullptr;
}

VirtualAuthenticatorManagerImpl*
AuthenticatorEnvironmentImpl::MaybeGetVirtualAuthenticatorManager(
    FrameTreeNode* node) {
  for (; node; node = FrameTreeNode::From(node->parent())) {
    if (base::Contains(virtual_authenticator_managers_, node)) {
      return virtual_authenticator_managers_[node].get();
    }
  }
  return nullptr;
}

void AuthenticatorEnvironmentImpl::AddVirtualAuthenticatorReceiver(
    FrameTreeNode* node,
    mojo::PendingReceiver<blink::test::mojom::VirtualAuthenticatorManager>
        receiver) {
  auto it = virtual_authenticator_managers_.find(node);
  DCHECK(it != virtual_authenticator_managers_.end());
  it->second->AddReceiver(std::move(receiver));
}

bool AuthenticatorEnvironmentImpl::HasVirtualUserVerifyingPlatformAuthenticator(
    FrameTreeNode* node) {
  VirtualAuthenticatorManagerImpl* authenticator_manager =
      MaybeGetVirtualAuthenticatorManager(node);
  if (!authenticator_manager) {
    return false;
  }
  std::vector<VirtualAuthenticator*> authenticators =
      authenticator_manager->GetAuthenticators();
  return std::any_of(authenticators.begin(), authenticators.end(),
                     [](VirtualAuthenticator* a) {
                       return a->is_user_verifying_platform_authenticator();
                     });
}

device::FidoDiscoveryFactory*
AuthenticatorEnvironmentImpl::MaybeGetDiscoveryFactoryTestOverride() {
  return replaced_discovery_factory_.get();
}

void AuthenticatorEnvironmentImpl::ReplaceDefaultDiscoveryFactoryForTesting(
    std::unique_ptr<device::FidoDiscoveryFactory> factory) {
  replaced_discovery_factory_ = std::move(factory);
}

void AuthenticatorEnvironmentImpl::OnFrameTreeNodeDestroyed(
    FrameTreeNode* node) {
  DisableVirtualAuthenticatorFor(node);
}

}  // namespace content
