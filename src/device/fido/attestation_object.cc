// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/attestation_object.h"

#include <utility>

#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "device/fido/attestation_statement.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/fido_constants.h"
#include "device/fido/opaque_attestation_statement.h"

namespace device {

// static
base::Optional<AttestationObject> AttestationObject::Parse(
    const cbor::Value& value) {
  if (!value.is_map()) {
    return base::nullopt;
  }
  const cbor::Value::MapValue& map = value.GetMap();

  const auto& format_it = map.find(cbor::Value(kFormatKey));
  if (format_it == map.end() || !format_it->second.is_string()) {
    return base::nullopt;
  }
  const std::string& fmt = format_it->second.GetString();

  const auto& att_stmt_it = map.find(cbor::Value(kAttestationStatementKey));
  if (att_stmt_it == map.end() || !att_stmt_it->second.is_map()) {
    return base::nullopt;
  }
  std::unique_ptr<AttestationStatement> attestation_statement =
      std::make_unique<OpaqueAttestationStatement>(
          fmt, cbor::Value(att_stmt_it->second.GetMap()));

  const auto& auth_data_it = map.find(cbor::Value(kAuthDataKey));
  if (auth_data_it == map.end() || !auth_data_it->second.is_bytestring()) {
    return base::nullopt;
  }
  base::Optional<AuthenticatorData> authenticator_data =
      AuthenticatorData::DecodeAuthenticatorData(
          auth_data_it->second.GetBytestring());
  if (!authenticator_data) {
    return base::nullopt;
  }
  return AttestationObject(std::move(*authenticator_data),
                           std::move(attestation_statement));
}

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

void AttestationObject::EraseAttestationStatement(
    AttestationObject::AAGUID erase_aaguid) {
  attestation_statement_ = std::make_unique<NoneAttestationStatement>();
  if (erase_aaguid == AAGUID::kErase) {
    authenticator_data_.DeleteDeviceAaguid();
  }

// Attested credential data is optional section within authenticator data. But
// if present, the first 16 bytes of it represents a device AAGUID which must
// be set to zeros for none attestation statement format, unless explicitly
// requested otherwise (we make an exception for platform authenticators).
#if DCHECK_IS_ON()
  if (!authenticator_data_.attested_data())
    return;
  DCHECK(erase_aaguid == AAGUID::kInclude ||
         authenticator_data_.attested_data()->IsAaguidZero());
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

cbor::Value AsCBOR(const AttestationObject& object) {
  cbor::Value::MapValue map;
  map[cbor::Value(kFormatKey)] =
      cbor::Value(object.attestation_statement().format_name());
  map[cbor::Value(kAuthDataKey)] =
      cbor::Value(object.authenticator_data().SerializeToByteArray());
  map[cbor::Value(kAttestationStatementKey)] =
      AsCBOR(object.attestation_statement());
  return cbor::Value(std::move(map));
}

}  // namespace device
