// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_ENVIRONMENT_IMPL_H_
#define CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_ENVIRONMENT_IMPL_H_

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/common/content_export.h"
#include "content/public/browser/authenticator_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/webauthn/virtual_authenticator.mojom-forward.h"

namespace device {
class FidoDiscoveryFactory;
#if defined(OS_WIN)
class WinWebAuthnApi;
#endif
}  // namespace device

namespace content {

class VirtualAuthenticatorManagerImpl;

// Allows enabling and disabling per-frame virtual environments for the Web
// Authentication API. Disabling the environment resets its state.
//
// This class is a singleton.
class CONTENT_EXPORT AuthenticatorEnvironmentImpl
    : public AuthenticatorEnvironment,
      FrameTreeNode::Observer {
 public:
  static AuthenticatorEnvironmentImpl* GetInstance();

  AuthenticatorEnvironmentImpl(const AuthenticatorEnvironmentImpl&) = delete;
  AuthenticatorEnvironmentImpl& operator=(const AuthenticatorEnvironmentImpl&) =
      delete;

  // Enables the scoped virtual authenticator environment for the |node| and its
  // descendants.
  // Does not have any effect if the |node| already has the virtual environment
  // enabled.
  void EnableVirtualAuthenticatorFor(FrameTreeNode* node);

  // Disables the scoped virtual authenticator environment for this |node|,
  // resetting the state. If the environment is set on one of the |node|'s
  // parents instead, this won't have any effect.
  void DisableVirtualAuthenticatorFor(FrameTreeNode* node);

  // Returns whether the virtual authenticator environment is enabled for
  // |node|.
  bool IsVirtualAuthenticatorEnabledFor(FrameTreeNode* node);

  // Returns the virtual fido discovery factory for the |node| if the virtual
  // environment is enabled for it, otherwise returns nullptr.
  VirtualAuthenticatorManagerImpl* MaybeGetVirtualAuthenticatorManager(
      FrameTreeNode* node);

  // Adds the receiver to the virtual authenticator enabled for the |node|. The
  // virtual authenticator must be enabled beforehand.
  void AddVirtualAuthenticatorReceiver(
      FrameTreeNode* node,
      mojo::PendingReceiver<blink::test::mojom::VirtualAuthenticatorManager>
          receiver);

  // Returns whether |node| has the virtual authenticator environment enabled
  // with a user-verifying platform installed in that environment.
  bool HasVirtualUserVerifyingPlatformAuthenticator(FrameTreeNode* node);

  // Returns the override installed by
  // ReplaceDefaultDiscoveryFactoryForTesting().
  device::FidoDiscoveryFactory* MaybeGetDiscoveryFactoryTestOverride();

#if defined(OS_WIN)
  // win_webauthn_api returns the WinWebAuthApi instance to be used for talking
  // to the Windows WebAuthn API. This is a testing seam that can be altered
  // with |SetWinWebAuthnApiForTesting|.
  device::WinWebAuthnApi* win_webauthn_api() const;

  // SetWinWebAuthnApiForTesting sets a testing override for |win_webauthn_api|.
  void SetWinWebAuthnApiForTesting(device::WinWebAuthnApi*);

  // ClearWinWebAuthnApiForTesting clears the testing override for
  // |win_webauthn_api|.
  void ClearWinWebAuthnApiForTesting();
#endif

  // AuthenticatorEnvironment:
  void ReplaceDefaultDiscoveryFactoryForTesting(
      std::unique_ptr<device::FidoDiscoveryFactory> factory) override;

  // FrameTreeNode::Observer:
  void OnFrameTreeNodeDestroyed(FrameTreeNode* node) override;

 protected:
  AuthenticatorEnvironmentImpl();
  ~AuthenticatorEnvironmentImpl() override;

 private:
  friend class base::NoDestructor<AuthenticatorEnvironmentImpl>;

  std::unique_ptr<device::FidoDiscoveryFactory> replaced_discovery_factory_;

  std::map<FrameTreeNode*, std::unique_ptr<VirtualAuthenticatorManagerImpl>>
      virtual_authenticator_managers_;

#if defined(OS_WIN)
  raw_ptr<device::WinWebAuthnApi> win_webauthn_api_for_testing_ = nullptr;
#endif
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_ENVIRONMENT_IMPL_H_
