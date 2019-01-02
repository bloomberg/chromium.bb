// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/attestation_object.h"

#include <utility>

#include "components/cbor/cbor_values.h"
#include "components/cbor/cbor_writer.h"
#include "device/fido/attestation_statement.h"
#include "device/fido/fido_constants.h"

namespace device {

AttestationObject::AttestationObject(
    AuthenticatorData data,
    std::unique_ptr<AttestationStatement> statement)
    : authenticator_data_(std::move(data)),
      attestation_statement_(std::move(statement)) {}

AttestationObject::AttestationObject(AttestationObject&& other) = default;
AttestationObject& AttestationObject::operator=(AttestationObject&& other) =
    default;

AttestationObject::~AttestationObject() = default;

std::vector<uint8_t> AttestationObject::GetCredentialId() const {
  return authenticator_data_.GetCredentialId();
}

void AttestationObject::EraseAttestationStatement() {
  attestation_statement_ = std::make_unique<NoneAttestationStatement>();
  authenticator_data_.DeleteDeviceAaguid();

// Attested credential data is optional section within authenticator data. But
// if present, the first 16 bytes of it represents a device AAGUID which must
// be set to zeros for none attestation statement format.
#if DCHECK_IS_ON()
  if (!authenticator_data_.attested_data())
    return;
  DCHECK(authenticator_data_.attested_data()->IsAaguidZero());
#endif
}

bool AttestationObject::IsSelfAttestation() {
  if (!attestation_statement_->IsSelfAttestation()) {
    return false;
  }
  // Self-attestation also requires that the AAGUID be zero. See
  // https://www.w3.org/TR/webauthn/#createCredential.
  return !authenticator_data_.attested_data() ||
         authenticator_data_.attested_data()->IsAaguidZero();
}

bool AttestationObject::IsAttestationCertificateInappropriatelyIdentifying() {
  return attestation_statement_
      ->IsAttestationCertificateInappropriatelyIdentifying();
}

std::vector<uint8_t> AttestationObject::SerializeToCBOREncodedBytes() const {
  cbor::CBORValue::MapValue map;
  map[cbor::CBORValue(kFormatKey)] =
      cbor::CBORValue(attestation_statement_->format_name());
  map[cbor::CBORValue(kAuthDataKey)] =
      cbor::CBORValue(authenticator_data_.SerializeToByteArray());
  map[cbor::CBORValue(kAttestationStatementKey)] =
      cbor::CBORValue(attestation_statement_->GetAsCBORMap());
  return cbor::CBORWriter::Write(cbor::CBORValue(std::move(map)))
      .value_or(std::vector<uint8_t>());
}

std::vector<uint8_t> SerializeToCtapStyleCborEncodedBytes(
    const AttestationObject& object) {
  cbor::CBORValue::MapValue map;
  map.emplace(1, object.attestation_statement().format_name());
  map.emplace(2, object.authenticator_data().SerializeToByteArray());
  map.emplace(3, object.attestation_statement().GetAsCBORMap());
  auto encoded_bytes = cbor::CBORWriter::Write(cbor::CBORValue(std::move(map)));
  DCHECK(encoded_bytes);
  return std::move(*encoded_bytes);
}

}  // namespace device
