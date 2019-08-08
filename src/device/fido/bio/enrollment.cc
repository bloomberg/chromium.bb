// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/bio/enrollment.h"

#include "components/cbor/diagnostic_writer.h"
#include "components/device_event_log/device_event_log.h"

namespace device {

// static
BioEnrollmentRequest BioEnrollmentRequest::ForGetModality() {
  return BioEnrollmentRequest(true);
}

// static
BioEnrollmentRequest BioEnrollmentRequest::ForGetSensorInfo() {
  return BioEnrollmentRequest(
      BioEnrollmentModality::kFingerprint,
      BioEnrollmentSubCommand::kGetFingerprintSensorInfo);
}

BioEnrollmentRequest::BioEnrollmentRequest(BioEnrollmentRequest&&) = default;

BioEnrollmentRequest::~BioEnrollmentRequest() = default;

BioEnrollmentRequest::BioEnrollmentRequest(bool get_modality_)
    : get_modality(get_modality_) {}

BioEnrollmentRequest::BioEnrollmentRequest(BioEnrollmentModality modality_,
                                           BioEnrollmentSubCommand subcommand_)
    : modality(modality_), subcommand(subcommand_) {}

// static
base::Optional<BioEnrollmentResponse> BioEnrollmentResponse::Parse(
    const base::Optional<cbor::Value>& cbor_response) {
  if (!cbor_response || !cbor_response->is_map()) {
    return base::nullopt;
  }

  const auto& response_map = cbor_response->GetMap();

  BioEnrollmentResponse response;

  // modality
  auto it = response_map.find(
      cbor::Value(static_cast<int>(BioEnrollmentResponseKey::kModality)));
  if (it != response_map.end()) {
    response.modality =
        static_cast<BioEnrollmentModality>(it->second.GetUnsigned());
  }

  // fingerprint kind
  it = response_map.find(cbor::Value(
      static_cast<int>(BioEnrollmentResponseKey::kFingerprintKind)));
  if (it != response_map.end()) {
    response.fingerprint_kind =
        static_cast<BioEnrollmentFingerprintKind>(it->second.GetUnsigned());
  }

  // max captures required for enroll
  it = response_map.find(cbor::Value(static_cast<int>(
      BioEnrollmentResponseKey::kMaxCaptureSamplesRequiredForEnroll)));
  if (it != response_map.end()) {
    response.max_samples_for_enroll = it->second.GetUnsigned();
  }

  return response;
}

std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
AsCTAPRequestValuePair(const BioEnrollmentRequest& request) {
  cbor::Value::MapValue map;

  using Key = BioEnrollmentRequestKey;

  if (request.modality) {
    map.emplace(static_cast<int>(Key::kModality),
                static_cast<int>(*request.modality));
  }

  if (request.subcommand) {
    map.emplace(static_cast<int>(Key::kSubCommand),
                static_cast<int>(*request.subcommand));
  }

  if (request.params) {
    map.emplace(static_cast<int>(Key::kSubCommandParams), *request.params);
  }

  if (request.pin_protocol) {
    map.emplace(static_cast<int>(Key::kPinProtocol), *request.pin_protocol);
  }

  if (request.pin_auth) {
    map.emplace(static_cast<int>(Key::kPinAuth), *request.pin_auth);
  }

  if (request.get_modality) {
    map.emplace(static_cast<int>(Key::kGetModality), *request.get_modality);
  }

  return {CtapRequestCommand::kAuthenticatorBioEnrollmentPreview,
          cbor::Value(std::move(map))};
}

}  // namespace device
