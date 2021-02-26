// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_PUBLIC_KEY_H_
#define DEVICE_FIDO_PUBLIC_KEY_H_

#include <stdint.h>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "base/optional.h"

namespace device {

// https://www.w3.org/TR/webauthn/#credentialpublickey
struct COMPONENT_EXPORT(DEVICE_FIDO) PublicKey {
  PublicKey(int32_t algorithm,
            base::span<const uint8_t> cbor_bytes,
            base::Optional<std::vector<uint8_t>> der_bytes);
  ~PublicKey();

  // algorithm contains the COSE algorithm identifier for this public key.
  const int32_t algorithm;

  // cose_key_bytes contains the credential public key as a COSE_Key map as
  // defined in Section 7 of https://tools.ietf.org/html/rfc8152.
  const std::vector<uint8_t> cose_key_bytes;

  // der_bytes contains an ASN.1, DER, SubjectPublicKeyInfo describing this
  // public key, if possible. (WebAuthn can negotiate the use of unknown
  // public-key algorithms so not all public keys can be transformed into SPKI
  // form.)
  const base::Optional<std::vector<uint8_t>> der_bytes;

 private:
  DISALLOW_COPY_AND_ASSIGN(PublicKey);
};

}  // namespace device

#endif  // DEVICE_FIDO_PUBLIC_KEY_H_
