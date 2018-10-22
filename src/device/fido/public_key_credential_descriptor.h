// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_PUBLIC_KEY_CREDENTIAL_DESCRIPTOR_H_
#define DEVICE_FIDO_PUBLIC_KEY_CREDENTIAL_DESCRIPTOR_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/optional.h"
#include "components/cbor/cbor_values.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_transport_protocol.h"

namespace device {

// Data structure containing public key credential type (string) and credential
// id (byte array) as specified in the CTAP spec. Used for exclude_list for
// AuthenticatorMakeCredential command and allow_list parameter for
// AuthenticatorGetAssertion command.
class COMPONENT_EXPORT(DEVICE_FIDO) PublicKeyCredentialDescriptor {
 public:
  static base::Optional<PublicKeyCredentialDescriptor> CreateFromCBORValue(
      const cbor::CBORValue& cbor);

  PublicKeyCredentialDescriptor(CredentialType credential_type,
                                std::vector<uint8_t> id);
  PublicKeyCredentialDescriptor(
      CredentialType credential_type,
      std::vector<uint8_t> id,
      base::flat_set<FidoTransportProtocol> transports);
  PublicKeyCredentialDescriptor(const PublicKeyCredentialDescriptor& other);
  PublicKeyCredentialDescriptor(PublicKeyCredentialDescriptor&& other);
  PublicKeyCredentialDescriptor& operator=(
      const PublicKeyCredentialDescriptor& other);
  PublicKeyCredentialDescriptor& operator=(
      PublicKeyCredentialDescriptor&& other);
  ~PublicKeyCredentialDescriptor();

  cbor::CBORValue ConvertToCBOR() const;

  CredentialType credential_type() const { return credential_type_; }
  const std::vector<uint8_t>& id() const { return id_; }
  const base::flat_set<FidoTransportProtocol>& transports() const {
    return transports_;
  }

 private:
  CredentialType credential_type_;
  std::vector<uint8_t> id_;
  base::flat_set<FidoTransportProtocol> transports_;
};

}  // namespace device

#endif  // DEVICE_FIDO_PUBLIC_KEY_CREDENTIAL_DESCRIPTOR_H_
