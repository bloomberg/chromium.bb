// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/opaque_attestation_statement.h"

#include <utility>

#include "components/cbor/cbor_values.h"

using cbor::CBORValue;

namespace device {

OpaqueAttestationStatement::OpaqueAttestationStatement(
    std::string attestation_format,
    CBORValue attestation_statement_map)
    : AttestationStatement(std::move(attestation_format)),
      attestation_statement_map_(std::move(attestation_statement_map)) {}

OpaqueAttestationStatement::~OpaqueAttestationStatement() = default;

// Returns the deep copied cbor map value of |attestation_statement_map_|.
CBORValue::MapValue OpaqueAttestationStatement::GetAsCBORMap() const {
  DCHECK(attestation_statement_map_.is_map());
  CBORValue::MapValue new_map;
  new_map.reserve(attestation_statement_map_.GetMap().size());
  for (const auto& map_it : attestation_statement_map_.GetMap()) {
    new_map.try_emplace(new_map.end(), map_it.first.Clone(),
                        map_it.second.Clone());
  }
  return new_map;
}

bool OpaqueAttestationStatement::IsSelfAttestation() {
  DCHECK(attestation_statement_map_.is_map());
  const CBORValue::MapValue& m(attestation_statement_map_.GetMap());
  const CBORValue alg("alg");
  const CBORValue sig("sig");

  return format_ == "packed" && m.size() == 2 && m.count(std::move(alg)) == 1 &&
         m.count(std::move(sig)) == 1;
}

bool OpaqueAttestationStatement::
    IsAttestationCertificateInappropriatelyIdentifying() {
  return false;
}

base::Optional<base::span<const uint8_t>>
OpaqueAttestationStatement::GetLeafCertificate() const {
  DCHECK(attestation_statement_map_.is_map());
  const CBORValue::MapValue& m(attestation_statement_map_.GetMap());
  const CBORValue x5c("x5c");
  const auto it = m.find(x5c);
  if (it == m.end() || !it->second.is_array()) {
    return base::nullopt;
  }

  const CBORValue::ArrayValue& certs = it->second.GetArray();
  if (certs.empty() || !certs[0].is_bytestring()) {
    return base::nullopt;
  }

  return certs[0].GetBytestring();
}

}  // namespace device
