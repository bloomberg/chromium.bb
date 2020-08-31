// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/ctap_make_credential_request.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "base/numerics/safe_conversions.h"
#include "components/cbor/values.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"

namespace device {

namespace {
bool IsMakeCredentialOptionMapFormatCorrect(
    const cbor::Value::MapValue& option_map) {
  return std::all_of(
      option_map.begin(), option_map.end(), [](const auto& param) {
        return param.first.is_string() &&
               (param.first.GetString() == kResidentKeyMapKey ||
                param.first.GetString() == kUserVerificationMapKey) &&
               param.second.is_bool();
      });
}

bool AreMakeCredentialRequestMapKeysCorrect(
    const cbor::Value::MapValue& request_map) {
  return std::all_of(
      request_map.begin(), request_map.end(), [](const auto& param) {
        return (param.first.is_integer() && 1u <= param.first.GetInteger() &&
                param.first.GetInteger() <= 9u);
      });
}

}  // namespace

// static
base::Optional<CtapMakeCredentialRequest> CtapMakeCredentialRequest::Parse(
    const cbor::Value::MapValue& request_map,
    const ParseOpts& opts) {
  if (!AreMakeCredentialRequestMapKeysCorrect(request_map))
    return base::nullopt;

  const auto client_data_hash_it = request_map.find(cbor::Value(1));
  if (client_data_hash_it == request_map.end() ||
      !client_data_hash_it->second.is_bytestring() ||
      client_data_hash_it->second.GetBytestring().size() !=
          kClientDataHashLength) {
    return base::nullopt;
  }
  base::span<const uint8_t, kClientDataHashLength> client_data_hash(
      client_data_hash_it->second.GetBytestring().data(),
      kClientDataHashLength);

  const auto rp_entity_it = request_map.find(cbor::Value(2));
  if (rp_entity_it == request_map.end() || !rp_entity_it->second.is_map())
    return base::nullopt;

  auto rp_entity =
      PublicKeyCredentialRpEntity::CreateFromCBORValue(rp_entity_it->second);
  if (!rp_entity)
    return base::nullopt;

  const auto user_entity_it = request_map.find(cbor::Value(3));
  if (user_entity_it == request_map.end() || !user_entity_it->second.is_map())
    return base::nullopt;

  auto user_entity = PublicKeyCredentialUserEntity::CreateFromCBORValue(
      user_entity_it->second);
  if (!user_entity)
    return base::nullopt;

  const auto credential_params_it = request_map.find(cbor::Value(4));
  if (credential_params_it == request_map.end())
    return base::nullopt;

  auto credential_params = PublicKeyCredentialParams::CreateFromCBORValue(
      credential_params_it->second);
  if (!credential_params)
    return base::nullopt;

  CtapMakeCredentialRequest request(
      /*client_data_json=*/std::string(), std::move(*rp_entity),
      std::move(*user_entity), std::move(*credential_params));
  request.client_data_hash = fido_parsing_utils::Materialize(client_data_hash);

  const auto exclude_list_it = request_map.find(cbor::Value(5));
  if (exclude_list_it != request_map.end()) {
    if (!exclude_list_it->second.is_array())
      return base::nullopt;

    const auto& credential_descriptors = exclude_list_it->second.GetArray();
    std::vector<PublicKeyCredentialDescriptor> exclude_list;
    for (const auto& credential_descriptor : credential_descriptors) {
      auto excluded_credential =
          PublicKeyCredentialDescriptor::CreateFromCBORValue(
              credential_descriptor);
      if (!excluded_credential)
        return base::nullopt;

      exclude_list.push_back(std::move(*excluded_credential));
    }
    request.exclude_list = std::move(exclude_list);
  }

  const auto extensions_it = request_map.find(cbor::Value(6));
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

      const std::string& extension_name = extension.first.GetString();

      if (extension_name == kExtensionCredProtect) {
        if (!extension.second.is_unsigned()) {
          return base::nullopt;
        }
        switch (extension.second.GetUnsigned()) {
          case 1:
            request.cred_protect = device::CredProtect::kUVOptional;
            break;
          case 2:
            request.cred_protect = device::CredProtect::kUVOrCredIDRequired;
            break;
          case 3:
            request.cred_protect = device::CredProtect::kUVRequired;
            break;
          default:
            return base::nullopt;
        }
      } else if (extension_name == kExtensionHmacSecret) {
        if (!extension.second.is_bool()) {
          return base::nullopt;
        }
        request.hmac_secret = extension.second.GetBool();
      } else if (extension_name == kExtensionAndroidClientData) {
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

  const auto option_it = request_map.find(cbor::Value(7));
  if (option_it != request_map.end()) {
    if (!option_it->second.is_map())
      return base::nullopt;

    const auto& option_map = option_it->second.GetMap();
    if (!IsMakeCredentialOptionMapFormatCorrect(option_map))
      return base::nullopt;

    const auto resident_key_option =
        option_map.find(cbor::Value(kResidentKeyMapKey));
    if (resident_key_option != option_map.end()) {
      request.resident_key_required = resident_key_option->second.GetBool();
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

  const auto pin_auth_it = request_map.find(cbor::Value(8));
  if (pin_auth_it != request_map.end()) {
    if (!pin_auth_it->second.is_bytestring())
      return base::nullopt;

    request.pin_auth = pin_auth_it->second.GetBytestring();
  }

  const auto pin_protocol_it = request_map.find(cbor::Value(9));
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

CtapMakeCredentialRequest::CtapMakeCredentialRequest(
    std::string in_client_data_json,
    PublicKeyCredentialRpEntity in_rp,
    PublicKeyCredentialUserEntity in_user,
    PublicKeyCredentialParams in_public_key_credential_params)
    : client_data_json(std::move(in_client_data_json)),
      client_data_hash(fido_parsing_utils::CreateSHA256Hash(client_data_json)),
      rp(std::move(in_rp)),
      user(std::move(in_user)),
      public_key_credential_params(std::move(in_public_key_credential_params)) {
}

CtapMakeCredentialRequest::CtapMakeCredentialRequest(
    const CtapMakeCredentialRequest& that) = default;

CtapMakeCredentialRequest::CtapMakeCredentialRequest(
    CtapMakeCredentialRequest&& that) = default;

CtapMakeCredentialRequest& CtapMakeCredentialRequest::operator=(
    const CtapMakeCredentialRequest& that) = default;

CtapMakeCredentialRequest& CtapMakeCredentialRequest::operator=(
    CtapMakeCredentialRequest&& that) = default;

CtapMakeCredentialRequest::~CtapMakeCredentialRequest() = default;

std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
AsCTAPRequestValuePair(const CtapMakeCredentialRequest& request) {
  cbor::Value::MapValue cbor_map;
  cbor_map[cbor::Value(1)] = cbor::Value(request.client_data_hash);
  cbor_map[cbor::Value(2)] = AsCBOR(request.rp);
  cbor_map[cbor::Value(3)] = AsCBOR(request.user);
  cbor_map[cbor::Value(4)] = AsCBOR(request.public_key_credential_params);
  if (!request.exclude_list.empty()) {
    cbor::Value::ArrayValue exclude_list_array;
    for (const auto& descriptor : request.exclude_list) {
      exclude_list_array.push_back(AsCBOR(descriptor));
    }
    cbor_map[cbor::Value(5)] = cbor::Value(std::move(exclude_list_array));
  }

  cbor::Value::MapValue extensions;

  if (request.hmac_secret) {
    extensions[cbor::Value(kExtensionHmacSecret)] = cbor::Value(true);
  }

  if (request.cred_protect) {
    extensions.emplace(kExtensionCredProtect,
                       static_cast<int64_t>(*request.cred_protect));
  }

  if (request.android_client_data_ext) {
    extensions.emplace(kExtensionAndroidClientData,
                       AsCBOR(*request.android_client_data_ext));
  }

  if (!extensions.empty()) {
    cbor_map[cbor::Value(6)] = cbor::Value(std::move(extensions));
  }

  if (request.pin_auth) {
    cbor_map[cbor::Value(8)] = cbor::Value(*request.pin_auth);
  }

  if (request.pin_protocol) {
    cbor_map[cbor::Value(9)] = cbor::Value(*request.pin_protocol);
  }

  cbor::Value::MapValue option_map;

  // Resident keys are not required by default.
  if (request.resident_key_required) {
    option_map[cbor::Value(kResidentKeyMapKey)] =
        cbor::Value(request.resident_key_required);
  }

  // User verification is not required by default.
  if (request.user_verification == UserVerificationRequirement::kRequired) {
    option_map[cbor::Value(kUserVerificationMapKey)] = cbor::Value(true);
  }

  if (!option_map.empty()) {
    cbor_map[cbor::Value(7)] = cbor::Value(std::move(option_map));
  }

  return std::make_pair(CtapRequestCommand::kAuthenticatorMakeCredential,
                        cbor::Value(std::move(cbor_map)));
}

}  // namespace device
