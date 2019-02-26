// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/ctap_make_credential_request.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "base/numerics/safe_conversions.h"
#include "components/cbor/writer.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"

namespace device {

CtapMakeCredentialRequest::CtapMakeCredentialRequest(
    std::string client_data_json,
    PublicKeyCredentialRpEntity rp,
    PublicKeyCredentialUserEntity user,
    PublicKeyCredentialParams public_key_credential_params)
    : client_data_json_(std::move(client_data_json)),
      client_data_hash_(
          fido_parsing_utils::CreateSHA256Hash(client_data_json_)),
      rp_(std::move(rp)),
      user_(std::move(user)),
      public_key_credential_params_(std::move(public_key_credential_params)) {}

CtapMakeCredentialRequest::CtapMakeCredentialRequest(
    const CtapMakeCredentialRequest& that) = default;

CtapMakeCredentialRequest::CtapMakeCredentialRequest(
    CtapMakeCredentialRequest&& that) = default;

CtapMakeCredentialRequest& CtapMakeCredentialRequest::operator=(
    const CtapMakeCredentialRequest& that) = default;

CtapMakeCredentialRequest& CtapMakeCredentialRequest::operator=(
    CtapMakeCredentialRequest&& that) = default;

CtapMakeCredentialRequest::~CtapMakeCredentialRequest() = default;

std::vector<uint8_t> CtapMakeCredentialRequest::EncodeAsCBOR() const {
  cbor::Value::MapValue cbor_map;
  cbor_map[cbor::Value(1)] = cbor::Value(client_data_hash_);
  cbor_map[cbor::Value(2)] = rp_.ConvertToCBOR();
  cbor_map[cbor::Value(3)] = user_.ConvertToCBOR();
  cbor_map[cbor::Value(4)] = public_key_credential_params_.ConvertToCBOR();
  if (exclude_list_) {
    cbor::Value::ArrayValue exclude_list_array;
    for (const auto& descriptor : *exclude_list_) {
      exclude_list_array.push_back(descriptor.ConvertToCBOR());
    }
    cbor_map[cbor::Value(5)] = cbor::Value(std::move(exclude_list_array));
  }

  if (hmac_secret_) {
    cbor::Value::MapValue extensions;
    extensions[cbor::Value(kExtensionHmacSecret)] = cbor::Value(true);
    cbor_map[cbor::Value(6)] = cbor::Value(std::move(extensions));
  }

  if (pin_auth_) {
    cbor_map[cbor::Value(8)] = cbor::Value(*pin_auth_);
  }

  if (pin_protocol_) {
    cbor_map[cbor::Value(9)] = cbor::Value(*pin_protocol_);
  }

  cbor::Value::MapValue option_map;

  // Resident keys are not required by default.
  if (resident_key_required_) {
    option_map[cbor::Value(kResidentKeyMapKey)] =
        cbor::Value(resident_key_required_);
  }

  // User verification is not required by default.
  if (user_verification_ == UserVerificationRequirement::kRequired) {
    option_map[cbor::Value(kUserVerificationMapKey)] = cbor::Value(true);
  }

  if (!option_map.empty()) {
    cbor_map[cbor::Value(7)] = cbor::Value(std::move(option_map));
  }

  auto serialized_param = cbor::Writer::Write(cbor::Value(std::move(cbor_map)));
  DCHECK(serialized_param);

  std::vector<uint8_t> cbor_request({base::strict_cast<uint8_t>(
      CtapRequestCommand::kAuthenticatorMakeCredential)});
  cbor_request.insert(cbor_request.end(), serialized_param->begin(),
                      serialized_param->end());
  return cbor_request;
}

CtapMakeCredentialRequest&
CtapMakeCredentialRequest::SetAuthenticatorAttachment(
    AuthenticatorAttachment authenticator_attachment) {
  authenticator_attachment_ = authenticator_attachment;
  return *this;
}

CtapMakeCredentialRequest& CtapMakeCredentialRequest::SetUserVerification(
    UserVerificationRequirement user_verification) {
  user_verification_ = user_verification;
  return *this;
}

CtapMakeCredentialRequest& CtapMakeCredentialRequest::SetResidentKeyRequired(
    bool resident_key_required) {
  resident_key_required_ = resident_key_required;
  return *this;
}

CtapMakeCredentialRequest& CtapMakeCredentialRequest::SetExcludeList(
    std::vector<PublicKeyCredentialDescriptor> exclude_list) {
  exclude_list_ = std::move(exclude_list);
  return *this;
}

CtapMakeCredentialRequest& CtapMakeCredentialRequest::SetPinAuth(
    std::vector<uint8_t> pin_auth) {
  pin_auth_ = std::move(pin_auth);
  return *this;
}

CtapMakeCredentialRequest& CtapMakeCredentialRequest::SetPinProtocol(
    uint8_t pin_protocol) {
  pin_protocol_ = pin_protocol;
  return *this;
}

CtapMakeCredentialRequest&
CtapMakeCredentialRequest::SetIsIndividualAttestation(
    bool is_individual_attestation) {
  is_individual_attestation_ = is_individual_attestation;
  return *this;
}

CtapMakeCredentialRequest& CtapMakeCredentialRequest::SetHmacSecret(
    bool hmac_secret) {
  hmac_secret_ = hmac_secret;
  return *this;
}

}  // namespace device
