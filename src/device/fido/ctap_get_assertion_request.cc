// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/ctap_get_assertion_request.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "base/numerics/safe_conversions.h"
#include "components/cbor/writer.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"

namespace device {

namespace {
bool IsGetAssertionOptionMapFormatCorrect(
    const cbor::Value::MapValue& option_map) {
  return std::all_of(
      option_map.begin(), option_map.end(), [](const auto& param) {
        return param.first.is_string() &&
               (param.first.GetString() == kUserPresenceMapKey ||
                param.first.GetString() == kUserVerificationMapKey) &&
               param.second.is_bool();
      });
}

bool AreGetAssertionRequestMapKeysCorrect(
    const cbor::Value::MapValue& request_map) {
  return std::all_of(
      request_map.begin(), request_map.end(), [](const auto& param) {
        return (param.first.is_integer() && 1u <= param.first.GetInteger() &&
                param.first.GetInteger() <= 7u);
      });
}
}  // namespace

// static
base::Optional<CtapGetAssertionRequest> CtapGetAssertionRequest::Parse(
    const cbor::Value::MapValue& request_map,
    const ParseOpts& opts) {
  if (!AreGetAssertionRequestMapKeysCorrect(request_map))
    return base::nullopt;

  const auto rp_id_it = request_map.find(cbor::Value(1));
  if (rp_id_it == request_map.end() || !rp_id_it->second.is_string())
    return base::nullopt;

  const auto client_data_hash_it = request_map.find(cbor::Value(2));
  if (client_data_hash_it == request_map.end() ||
      !client_data_hash_it->second.is_bytestring() ||
      client_data_hash_it->second.GetBytestring().size() !=
          kClientDataHashLength) {
    return base::nullopt;
  }
  base::span<const uint8_t, kClientDataHashLength> client_data_hash(
      client_data_hash_it->second.GetBytestring().data(),
      kClientDataHashLength);

  CtapGetAssertionRequest request(rp_id_it->second.GetString(),
                                  /*client_data_json=*/std::string());
  request.client_data_hash = fido_parsing_utils::Materialize(client_data_hash);

  const auto allow_list_it = request_map.find(cbor::Value(3));
  if (allow_list_it != request_map.end()) {
    if (!allow_list_it->second.is_array())
      return base::nullopt;

    const auto& credential_descriptors = allow_list_it->second.GetArray();
    if (credential_descriptors.empty())
      return base::nullopt;

    std::vector<PublicKeyCredentialDescriptor> allow_list;
    for (const auto& credential_descriptor : credential_descriptors) {
      auto allowed_credential =
          PublicKeyCredentialDescriptor::CreateFromCBORValue(
              credential_descriptor);
      if (!allowed_credential)
        return base::nullopt;

      allow_list.push_back(std::move(*allowed_credential));
    }
    request.allow_list = std::move(allow_list);
  }

  const auto extensions_it = request_map.find(cbor::Value(4));
  if (extensions_it != request_map.end()) {
    if (!extensions_it->second.is_map()) {
      return base::nullopt;
    }

    const cbor::Value::MapValue& extensions = extensions_it->second.GetMap();

    if (opts.reject_all_extensions && !extensions.empty()) {
      return base::nullopt;
    }

    for (const auto& extension : extensions) {
      if (!extension.first.is_string()) {
        return base::nullopt;
      }

      if (extension.first.GetString() == kExtensionAndroidClientData) {
        base::Optional<AndroidClientDataExtensionInput>
            android_client_data_ext =
                AndroidClientDataExtensionInput::Parse(extension.second);
        if (!android_client_data_ext) {
          return base::nullopt;
        }
        request.android_client_data_ext = std::move(*android_client_data_ext);
      }
    }
  }

  const auto option_it = request_map.find(cbor::Value(5));
  if (option_it != request_map.end()) {
    if (!option_it->second.is_map())
      return base::nullopt;

    const auto& option_map = option_it->second.GetMap();
    if (!IsGetAssertionOptionMapFormatCorrect(option_map))
      return base::nullopt;

    const auto user_presence_option =
        option_map.find(cbor::Value(kUserPresenceMapKey));
    if (user_presence_option != option_map.end()) {
      request.user_presence_required = user_presence_option->second.GetBool();
    }

    const auto uv_option =
        option_map.find(cbor::Value(kUserVerificationMapKey));
    if (uv_option != option_map.end()) {
      request.user_verification =
          uv_option->second.GetBool()
              ? UserVerificationRequirement::kRequired
              : UserVerificationRequirement::kDiscouraged;
    }
  }

  const auto pin_auth_it = request_map.find(cbor::Value(6));
  if (pin_auth_it != request_map.end()) {
    if (!pin_auth_it->second.is_bytestring())
      return base::nullopt;

    request.pin_auth = pin_auth_it->second.GetBytestring();
  }

  const auto pin_protocol_it = request_map.find(cbor::Value(7));
  if (pin_protocol_it != request_map.end()) {
    if (!pin_protocol_it->second.is_unsigned() ||
        pin_protocol_it->second.GetUnsigned() >
            std::numeric_limits<uint8_t>::max()) {
      return base::nullopt;
    }
    request.pin_protocol = pin_protocol_it->second.GetUnsigned();
  }

  return request;
}

CtapGetAssertionRequest::CtapGetAssertionRequest(
    std::string in_rp_id,
    std::string in_client_data_json)
    : rp_id(std::move(in_rp_id)),
      client_data_json(std::move(in_client_data_json)),
      client_data_hash(fido_parsing_utils::CreateSHA256Hash(client_data_json)) {
}

CtapGetAssertionRequest::CtapGetAssertionRequest(
    const CtapGetAssertionRequest& that) = default;

CtapGetAssertionRequest::CtapGetAssertionRequest(
    CtapGetAssertionRequest&& that) = default;

CtapGetAssertionRequest& CtapGetAssertionRequest::operator=(
    const CtapGetAssertionRequest& other) = default;

CtapGetAssertionRequest& CtapGetAssertionRequest::operator=(
    CtapGetAssertionRequest&& other) = default;

CtapGetAssertionRequest::~CtapGetAssertionRequest() = default;

std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
AsCTAPRequestValuePair(const CtapGetAssertionRequest& request) {
  cbor::Value::MapValue cbor_map;
  cbor_map[cbor::Value(1)] = cbor::Value(request.rp_id);
  cbor_map[cbor::Value(2)] = cbor::Value(request.client_data_hash);

  if (!request.allow_list.empty()) {
    cbor::Value::ArrayValue allow_list_array;
    for (const auto& descriptor : request.allow_list) {
      allow_list_array.push_back(AsCBOR(descriptor));
    }
    cbor_map[cbor::Value(3)] = cbor::Value(std::move(allow_list_array));
  }

  if (request.android_client_data_ext) {
    cbor::Value::MapValue extensions;
    extensions.emplace(kExtensionAndroidClientData,
                       AsCBOR(*request.android_client_data_ext));
    cbor_map[cbor::Value(4)] = cbor::Value(std::move(extensions));
  }

  if (request.pin_auth) {
    cbor_map[cbor::Value(6)] = cbor::Value(*request.pin_auth);
  }

  if (request.pin_protocol) {
    cbor_map[cbor::Value(7)] = cbor::Value(*request.pin_protocol);
  }

  cbor::Value::MapValue option_map;

  // User presence is required by default.
  if (!request.user_presence_required) {
    option_map[cbor::Value(kUserPresenceMapKey)] =
        cbor::Value(request.user_presence_required);
  }

  // User verification is not required by default.
  if (request.user_verification == UserVerificationRequirement::kRequired) {
    option_map[cbor::Value(kUserVerificationMapKey)] = cbor::Value(true);
  }

  if (!option_map.empty()) {
    cbor_map[cbor::Value(5)] = cbor::Value(std::move(option_map));
  }

  return std::make_pair(CtapRequestCommand::kAuthenticatorGetAssertion,
                        cbor::Value(std::move(cbor_map)));
}

std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
AsCTAPRequestValuePair(const CtapGetNextAssertionRequest&) {
  return std::make_pair(CtapRequestCommand::kAuthenticatorGetNextAssertion,
                        base::nullopt);
}

}  // namespace device
