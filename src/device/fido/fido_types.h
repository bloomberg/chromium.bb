// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FIDO_TYPES_H_
#define DEVICE_FIDO_FIDO_TYPES_H_

#include <cstdint>

// The definitions below are for mojo-mappable types that need to be transferred
// from Blink. Types that do not have mojo equivalents are better placed in
// fido_constants.h.

namespace device {

// ProtocolVersion is the major protocol version of an authenticator device.
enum class ProtocolVersion {
  kCtap2,
  kU2f,
  kUnknown,
};

// Ctap2Version distinguishes different minor versions of the CTAP2 protocol.
enum class Ctap2Version {
  kCtap2_0,
  kCtap2_1,
};

enum class CredentialType { kPublicKey };

// Authenticator attachment constraint passed on from the relying party as a
// parameter for AuthenticatorSelectionCriteria. |kAny| is equivalent to the
// (optional) attachment field not being present.
// https://w3c.github.io/webauthn/#attachment
enum class AuthenticatorAttachment {
  kAny,
  kPlatform,
  kCrossPlatform,
};

// A constraint on whether a client-side discoverable (resident) credential
// should be created during registration.
//
// https://w3c.github.io/webauthn/#enum-residentKeyRequirement
enum class ResidentKeyRequirement {
  kDiscouraged,
  kPreferred,
  kRequired,
};

// User verification constraint passed on from the relying party as a parameter
// for AuthenticatorSelectionCriteria and for CtapGetAssertion request.
// https://w3c.github.io/webauthn/#enumdef-userverificationrequirement
enum class UserVerificationRequirement {
  kRequired,
  kPreferred,
  kDiscouraged,
};

// https://w3c.github.io/webauthn/#attestation-convey
enum class AttestationConveyancePreference : uint8_t {
  kNone,
  kIndirect,
  kDirect,
  // The value "enterprise" from WebAuthn is split into these two values. The
  // first indicates that the site requested enterprise attestation and we can
  // pass that onto the authenticator in case the RP ID is hardcoded into the
  // authenticator to allow that. The second indicates that we, the browser,
  // have authenticated the request (e.g. by enterprise policy) and it's
  // permitted without further checks.
  kEnterpriseIfRPListedOnAuthenticator,
  kEnterpriseApprovedByBrowser,
};

// https://w3c.github.io/webauthn#enumdef-largeblobsupport
enum class LargeBlobSupport {
  kNotRequested,
  kRequired,
  kPreferred,
};

}  // namespace device

#endif  // DEVICE_FIDO_FIDO_TYPES_H_
