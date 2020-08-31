// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/authenticator_reference.h"

AuthenticatorReference::AuthenticatorReference(
    base::StringPiece authenticator_id,
    base::StringPiece16 authenticator_display_name,
    device::FidoTransportProtocol transport)
    : authenticator_id(authenticator_id),
      authenticator_display_name(authenticator_display_name),
      transport(transport) {}

AuthenticatorReference::AuthenticatorReference(AuthenticatorReference&& data) =
    default;

AuthenticatorReference& AuthenticatorReference::operator=(
    AuthenticatorReference&& other) = default;

AuthenticatorReference::~AuthenticatorReference() = default;
