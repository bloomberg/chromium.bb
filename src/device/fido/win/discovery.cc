// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/win/discovery.h"

#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace device {

WinWebAuthnApiAuthenticatorDiscovery::WinWebAuthnApiAuthenticatorDiscovery(
    WinWebAuthnApi* const win_webauthn_api,
    HWND parent_window)
    : FidoDiscoveryBase(FidoTransportProtocol::kUsbHumanInterfaceDevice),
      win_webauthn_api_(win_webauthn_api),
      parent_window_(parent_window),
      weak_factory_(this) {}

WinWebAuthnApiAuthenticatorDiscovery::~WinWebAuthnApiAuthenticatorDiscovery() =
    default;

void WinWebAuthnApiAuthenticatorDiscovery::Start() {
  DCHECK(!authenticator_);
  if (!observer()) {
    return;
  }

  if (!win_webauthn_api_->IsAvailable()) {
    observer()->DiscoveryStarted(this, false /* discovery failed */);
    return;
  }

  observer()->DiscoveryStarted(this, true /* success */);

  // Start() is currently invoked synchronously in the
  // FidoRequestHandler ctor. Invoke AddAuthenticator() asynchronously
  // to avoid hairpinning FidoRequestHandler::AuthenticatorAdded()
  // before the request handler has an observer.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&WinWebAuthnApiAuthenticatorDiscovery::AddAuthenticator,
                     weak_factory_.GetWeakPtr()));
}

void WinWebAuthnApiAuthenticatorDiscovery::AddAuthenticator() {
  authenticator_ = std::make_unique<WinWebAuthnApiAuthenticator>(
      WinWebAuthnApi::GetDefault(), parent_window_);
  observer()->AuthenticatorAdded(this, authenticator_.get());
}

}  // namespace device
