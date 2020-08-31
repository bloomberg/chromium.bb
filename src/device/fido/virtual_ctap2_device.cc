// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/virtual_ctap2_device.h"

#include <algorithm>
#include <array>
#include <string>
#include <utility>

#include "base/base64url.h"
#include "base/bind.h"
#include "base/containers/span.h"
#include "base/json/string_escape.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/cbor/reader.h"
#include "components/cbor/writer.h"
#include "crypto/ec_private_key.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/bio/enrollment.h"
#include "device/fido/credential_management.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/opaque_attestation_statement.h"
#include "device/fido/p256_public_key.h"
#include "device/fido/pin.h"
#include "device/fido/pin_internal.h"
#include "device/fido/public_key.h"
#include "device/fido/virtual_u2f_device.h"
#include "third_party/boringssl/src/include/openssl/aes.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/hmac.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/obj.h"
#include "third_party/boringssl/src/include/openssl/rand.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

namespace device {

namespace {

constexpr std::array<uint8_t, kAaguidLength> kDeviceAaguid = {
    {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x01, 0x02, 0x03, 0x04,
     0x05, 0x06, 0x07, 0x08}};

std::vector<uint8_t> ConstructResponse(CtapDeviceResponseCode response_code,
                                       base::span<const uint8_t> data) {
  std::vector<uint8_t> response{base::strict_cast<uint8_t>(response_code)};
  fido_parsing_utils::Append(&response, data);
  return response;
}

void ReturnCtap2Response(
    FidoDevice::DeviceCallback cb,
    CtapDeviceResponseCode response_code,
    base::Optional<base::span<const uint8_t>> data = base::nullopt) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(cb),
                     ConstructResponse(response_code,
                                       data.value_or(std::vector<uint8_t>{}))));
}

// CheckPINToken returns true iff |pin_auth| is a valid authentication of
// |data| given that the PIN token in effect is |pin_token|.
bool CheckPINToken(base::span<const uint8_t> pin_token,
                   base::span<const uint8_t> pin_auth,
                   base::span<const uint8_t> data) {
  uint8_t calculated_pin_auth[SHA256_DIGEST_LENGTH];
  unsigned hmac_bytes;
  CHECK(HMAC(EVP_sha256(), pin_token.data(), pin_token.size(), data.data(),
             data.size(), calculated_pin_auth, &hmac_bytes));
  DCHECK_EQ(sizeof(calculated_pin_auth), static_cast<size_t>(hmac_bytes));

  return pin_auth.size() == 16 &&
         CRYPTO_memcmp(pin_auth.data(), calculated_pin_auth, 16) == 0;
}

std::vector<uint8_t> ConstructSignatureBuffer(
    const AuthenticatorData& authenticator_data,
    base::span<const uint8_t, kClientDataHashLength> client_data_hash) {
  std::vector<uint8_t> signature_buffer;
  fido_parsing_utils::Append(&signature_buffer,
                             authenticator_data.SerializeToByteArray());
  fido_parsing_utils::Append(&signature_buffer, client_data_hash);
  return signature_buffer;
}

std::string ConstructAndroidClientDataJSON(
    const AndroidClientDataExtensionInput& input,
    base::StringPiece type) {
  std::string challenge_b64url;
  base::Base64UrlEncode(
      base::StringPiece(reinterpret_cast<const char*>(input.challenge.data()),
                        input.challenge.size()),
      base::Base64UrlEncodePolicy::OMIT_PADDING, &challenge_b64url);
  return "{\"challenge\":" + base::GetQuotedJSONString(challenge_b64url) +
         ",\"origin\":" + base::GetQuotedJSONString(input.origin.Serialize()) +
         ",\"type\":" + base::GetQuotedJSONString(type) +
         ",\"androidPackageName\":\"org.chromium.device.fido.test\"}";
}

std::vector<uint8_t> ConstructMakeCredentialResponse(
    const base::Optional<std::vector<uint8_t>> attestation_certificate,
    base::span<const uint8_t> signature,
    AuthenticatorData authenticator_data,
    base::Optional<std::vector<uint8_t>> android_client_data_ext) {
  cbor::Value::MapValue attestation_map;
  attestation_map.emplace("alg", -7);
  attestation_map.emplace("sig", fido_parsing_utils::Materialize(signature));

  if (attestation_certificate) {
    cbor::Value::ArrayValue certificate_chain;
    certificate_chain.emplace_back(std::move(*attestation_certificate));
    attestation_map.emplace("x5c", std::move(certificate_chain));
  }

  AuthenticatorMakeCredentialResponse make_credential_response(
      FidoTransportProtocol::kUsbHumanInterfaceDevice,
      AttestationObject(
          std::move(authenticator_data),
          std::make_unique<OpaqueAttestationStatement>(
              "packed", cbor::Value(std::move(attestation_map)))));
  if (android_client_data_ext) {
    make_credential_response.set_android_client_data_ext(
        *android_client_data_ext);
  }
  return AsCTAPStyleCBORBytes(make_credential_response);
}

base::Optional<std::vector<uint8_t>> GetPINBytestring(
    const cbor::Value::MapValue& request,
    pin::RequestKey key) {
  const auto it = request.find(cbor::Value(static_cast<int>(key)));
  if (it == request.end() || !it->second.is_bytestring()) {
    return base::nullopt;
  }
  return it->second.GetBytestring();
}

base::Optional<bssl::UniquePtr<EC_POINT>> GetPINKey(
    const cbor::Value::MapValue& request,
    pin::RequestKey map_key) {
  const auto it = request.find(cbor::Value(static_cast<int>(map_key)));
  if (it == request.end() || !it->second.is_map()) {
    return base::nullopt;
  }
  const auto& cose_key = it->second.GetMap();
  auto response = pin::KeyAgreementResponse::ParseFromCOSE(cose_key);
  if (!response) {
    return base::nullopt;
  }

  bssl::UniquePtr<EC_GROUP> group(
      EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
  return pin::PointFromKeyAgreementResponse(group.get(), *response).value();
}

// ConfirmPresentedPIN checks whether |encrypted_pin_hash| is a valid proof-of-
// possession of the PIN, given that |shared_key| is the result of the ECDH key
// agreement.
CtapDeviceResponseCode ConfirmPresentedPIN(
    VirtualCtap2Device::State* state,
    const uint8_t shared_key[SHA256_DIGEST_LENGTH],
    const std::vector<uint8_t>& encrypted_pin_hash) {
  if (state->pin_retries == 0) {
    return CtapDeviceResponseCode::kCtap2ErrPinBlocked;
  }
  if (state->soft_locked) {
    return CtapDeviceResponseCode::kCtap2ErrPinAuthBlocked;
  }

  state->pin_retries--;
  state->pin_retries_since_insertion++;

  DCHECK((encrypted_pin_hash.size() % AES_BLOCK_SIZE) == 0);
  uint8_t pin_hash[AES_BLOCK_SIZE];
  pin::Decrypt(shared_key, encrypted_pin_hash, pin_hash);

  uint8_t calculated_pin_hash[SHA256_DIGEST_LENGTH];
  SHA256(reinterpret_cast<const uint8_t*>(state->pin.data()), state->pin.size(),
         calculated_pin_hash);

  if (state->pin.empty() ||
      CRYPTO_memcmp(pin_hash, calculated_pin_hash, sizeof(pin_hash)) != 0) {
    if (state->pin_retries == 0) {
      return CtapDeviceResponseCode::kCtap2ErrPinBlocked;
    }
    if (state->pin_retries_since_insertion == 3) {
      state->soft_locked = true;
      return CtapDeviceResponseCode::kCtap2ErrPinAuthBlocked;
    }
    return CtapDeviceResponseCode::kCtap2ErrPinInvalid;
  }

  state->pin_retries = kMaxPinRetries;
  state->uv_retries = kMaxUvRetries;
  state->pin_retries_since_insertion = 0;

  return CtapDeviceResponseCode::kSuccess;
}

// SetPIN sets the current PIN based on the ciphertext in |encrypted_pin|, given
// that |shared_key| is the result of the ECDH key agreement.
CtapDeviceResponseCode SetPIN(VirtualCtap2Device::State* state,
                              const uint8_t shared_key[SHA256_DIGEST_LENGTH],
                              const std::vector<uint8_t>& encrypted_pin,
                              const std::vector<uint8_t>& pin_auth) {
  // See
  // https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html#settingNewPin
  uint8_t calculated_pin_auth[SHA256_DIGEST_LENGTH];
  unsigned hmac_bytes;
  CHECK(HMAC(EVP_sha256(), shared_key, SHA256_DIGEST_LENGTH,
             encrypted_pin.data(), encrypted_pin.size(), calculated_pin_auth,
             &hmac_bytes));
  DCHECK_EQ(sizeof(calculated_pin_auth), static_cast<size_t>(hmac_bytes));

  static_assert(sizeof(calculated_pin_auth) >= 16,
                "calculated_pin_auth is expected to be at least 16 bytes");
  if (pin_auth.size() != 16 ||
      CRYPTO_memcmp(calculated_pin_auth, pin_auth.data(), pin_auth.size()) !=
          0) {
    return CtapDeviceResponseCode::kCtap2ErrPinAuthInvalid;
  }

  if (encrypted_pin.size() < 64) {
    return CtapDeviceResponseCode::kCtap2ErrPinPolicyViolation;
  }

  std::vector<uint8_t> plaintext_pin(encrypted_pin.size());
  pin::Decrypt(shared_key, encrypted_pin, plaintext_pin.data());

  size_t padding_len = 0;
  while (padding_len < plaintext_pin.size() &&
         plaintext_pin[plaintext_pin.size() - padding_len - 1] == 0) {
    padding_len++;
  }

  plaintext_pin.resize(plaintext_pin.size() - padding_len);
  if (padding_len == 0 || plaintext_pin.size() < 4 ||
      plaintext_pin.size() > 63) {
    return CtapDeviceResponseCode::kCtap2ErrPinPolicyViolation;
  }

  state->pin = std::string(reinterpret_cast<const char*>(plaintext_pin.data()),
                           plaintext_pin.size());
  state->pin_retries = kMaxPinRetries;
  state->uv_retries = kMaxUvRetries;

  return CtapDeviceResponseCode::kSuccess;
}

CtapDeviceResponseCode CheckCredentialManagementPINAuth(
    const cbor::Value::MapValue& request_map,
    base::span<const uint8_t> pin_token,
    base::span<const uint8_t> pinauth_bytes) {
  const auto pin_protocol_it = request_map.find(cbor::Value(
      static_cast<int>(CredentialManagementRequestKey::kPinProtocol)));
  if (pin_protocol_it == request_map.end() ||
      !pin_protocol_it->second.is_unsigned()) {
    return CtapDeviceResponseCode::kCtap2ErrCBORUnexpectedType;
  }
  if (pin_protocol_it->second.GetUnsigned() != pin::kProtocolVersion) {
    return CtapDeviceResponseCode::kCtap2ErrInvalidOption;
  }
  const auto pinauth_it = request_map.find(
      cbor::Value(static_cast<int>(CredentialManagementRequestKey::kPinAuth)));
  if (pinauth_it == request_map.end() || !pinauth_it->second.is_bytestring()) {
    return CtapDeviceResponseCode::kCtap2ErrCBORUnexpectedType;
  }
  if (!CheckPINToken(pin_token, pinauth_it->second.GetBytestring(),
                     pinauth_bytes)) {
    return CtapDeviceResponseCode::kCtap2ErrPinAuthInvalid;
  }
  return CtapDeviceResponseCode::kSuccess;
}

// Like AsCBOR(const PublicKeyCredentialRpEntity&), but optionally allows name
// to be INVALID_UTF8.
base::Optional<cbor::Value> RpEntityAsCBOR(
    const PublicKeyCredentialRpEntity& rp,
    bool allow_invalid_utf8) {
  if (!allow_invalid_utf8) {
    return AsCBOR(rp);
  }

  cbor::Value::MapValue rp_map;
  rp_map.emplace(kEntityIdMapKey, rp.id);
  if (rp.name) {
    rp_map.emplace(kEntityNameMapKey,
                   cbor::Value::InvalidUTF8StringValueForTesting(*rp.name));
  }
  if (rp.icon_url) {
    rp_map.emplace(kIconUrlMapKey, rp.icon_url->spec());
  }
  return cbor::Value(std::move(rp_map));
}

// Like AsCBOR(const PublicKeyCredentialUserEntity&), but optionally allows name
// or displayName to be INVALID_UTF8.
base::Optional<cbor::Value> UserEntityAsCBOR(
    const PublicKeyCredentialUserEntity& user,
    bool allow_invalid_utf8) {
  if (!allow_invalid_utf8) {
    return AsCBOR(user);
  }

  cbor::Value::MapValue user_map;
  user_map.emplace(kEntityIdMapKey, user.id);
  if (user.name) {
    user_map.emplace(kEntityNameMapKey,
                     cbor::Value::InvalidUTF8StringValueForTesting(*user.name));
  }
  // Empty icon URLs result in CTAP1_ERR_INVALID_LENGTH on some security keys.
  if (user.icon_url && !user.icon_url->is_empty()) {
    user_map.emplace(kIconUrlMapKey, user.icon_url->spec());
  }
  if (user.display_name) {
    user_map.emplace(
        kDisplayNameMapKey,
        cbor::Value::InvalidUTF8StringValueForTesting(*user.display_name));
  }
  return cbor::Value(std::move(user_map));
}

std::vector<uint8_t> WriteCBOR(cbor::Value value,
                               bool allow_invalid_utf8 = false) {
  cbor::Writer::Config config;
  config.allow_invalid_utf8_for_testing = allow_invalid_utf8;
  return *cbor::Writer::Write(std::move(value), std::move(config));
}

std::vector<uint8_t> EncodeGetAssertionResponse(
    const AuthenticatorGetAssertionResponse& response,
    bool allow_invalid_utf8) {
  cbor::Value::MapValue response_map;
  if (response.credential()) {
    response_map.emplace(1, AsCBOR(*response.credential()));
  }

  response_map.emplace(2, response.auth_data().SerializeToByteArray());
  response_map.emplace(3, response.signature());

  if (response.user_entity()) {
    response_map.emplace(
        4, *UserEntityAsCBOR(*response.user_entity(), allow_invalid_utf8));
  }
  if (response.num_credentials()) {
    response_map.emplace(5, response.num_credentials().value());
  }
  if (response.android_client_data_ext()) {
    response_map.emplace(0xf0,
                         cbor::Value(*response.android_client_data_ext()));
  }

  return WriteCBOR(cbor::Value(std::move(response_map)), allow_invalid_utf8);
}

std::array<uint8_t, 32> GenerateAndEncryptToken(
    base::span<const uint8_t, SHA256_DIGEST_LENGTH> shared_key,
    base::span<uint8_t, 32> pin_token) {
  RAND_bytes(pin_token.data(), pin_token.size());
  std::array<uint8_t, pin_token.size()> encrypted_pin_token;
  pin::Encrypt(shared_key.data(), pin_token, encrypted_pin_token.data());
  return encrypted_pin_token;
}

}  // namespace

VirtualCtap2Device::Config::Config() = default;
VirtualCtap2Device::Config::Config(const Config&) = default;
VirtualCtap2Device::Config& VirtualCtap2Device::Config::operator=(
    const Config&) = default;
VirtualCtap2Device::Config::~Config() = default;

VirtualCtap2Device::VirtualCtap2Device() : VirtualFidoDevice() {
  device_info_ =
      AuthenticatorGetInfoResponse({ProtocolVersion::kCtap2}, kDeviceAaguid);
  device_info_->algorithms = {
      static_cast<int32_t>(CoseAlgorithmIdentifier::kCoseEs256),
      static_cast<int32_t>(CoseAlgorithmIdentifier::kCoseRs256),
  };
}

VirtualCtap2Device::VirtualCtap2Device(scoped_refptr<State> state,
                                       const Config& config)
    : VirtualFidoDevice(std::move(state)), config_(config) {
  std::vector<ProtocolVersion> versions = {ProtocolVersion::kCtap2};
  if (config.u2f_support) {
    versions.emplace_back(ProtocolVersion::kU2f);
    u2f_device_.reset(new VirtualU2fDevice(NewReferenceToState()));
  }
  device_info_ =
      AuthenticatorGetInfoResponse(std::move(versions), kDeviceAaguid);

  AuthenticatorSupportedOptions options;
  bool options_updated = false;
  if (config.pin_support) {
    options_updated = true;

    if (mutable_state()->pin.empty()) {
      options.client_pin_availability = AuthenticatorSupportedOptions::
          ClientPinAvailability::kSupportedButPinNotSet;
    } else {
      options.client_pin_availability = AuthenticatorSupportedOptions::
          ClientPinAvailability::kSupportedAndPinSet;
    }
  }

  if (config.internal_uv_support) {
    options_updated = true;
    if (mutable_state()->fingerprints_enrolled) {
      options.user_verification_availability = AuthenticatorSupportedOptions::
          UserVerificationAvailability::kSupportedAndConfigured;
    } else {
      options.user_verification_availability = AuthenticatorSupportedOptions::
          UserVerificationAvailability::kSupportedButNotConfigured;
    }
    options.supports_uv_token = config.uv_token_support;
  }

  if (config.resident_key_support) {
    options_updated = true;
    options.supports_resident_key = true;
  }

  if (config.credential_management_support) {
    options_updated = true;
    options.supports_credential_management = true;
  }

  if (config.bio_enrollment_support) {
    options_updated = true;
    if (mutable_state()->bio_enrollment_provisioned) {
      options.bio_enrollment_availability = AuthenticatorSupportedOptions::
          BioEnrollmentAvailability::kSupportedAndProvisioned;
    } else {
      options.bio_enrollment_availability = AuthenticatorSupportedOptions::
          BioEnrollmentAvailability::kSupportedButUnprovisioned;
    }
  }

  if (config.bio_enrollment_preview_support) {
    options_updated = true;
    if (mutable_state()->bio_enrollment_provisioned) {
      options.bio_enrollment_availability_preview =
          AuthenticatorSupportedOptions::BioEnrollmentAvailability::
              kSupportedAndProvisioned;
    } else {
      options.bio_enrollment_availability_preview =
          AuthenticatorSupportedOptions::BioEnrollmentAvailability::
              kSupportedButUnprovisioned;
    }
  }

  if (config.is_platform_authenticator) {
    options_updated = true;
    options.is_platform_device = true;
  }

  if (config.cred_protect_support) {
    options_updated = true;
    options.default_cred_protect = config.default_cred_protect;
  }

  if (config.support_android_client_data_extension) {
    options_updated = true;
    options.supports_android_client_data_ext = true;
  }

  if (options_updated) {
    device_info_->options = std::move(options);
  }

  if (config.cred_protect_support) {
    device_info_->extensions.emplace(
        {std::string(device::kExtensionCredProtect)});
  }

  if (config.support_android_client_data_extension) {
    device_info_->extensions.emplace(
        {std::string(device::kExtensionAndroidClientData)});
  }

  if (config.max_credential_count_in_list > 0) {
    device_info_->max_credential_count_in_list =
        config.max_credential_count_in_list;
  }

  if (config.max_credential_id_length > 0) {
    device_info_->max_credential_id_length = config.max_credential_id_length;
  }
}

VirtualCtap2Device::~VirtualCtap2Device() = default;

// As all operations for VirtualCtap2Device are synchronous and we do not wait
// for user touch, Cancel command is no-op.
void VirtualCtap2Device::Cancel(CancelToken) {}

FidoDevice::CancelToken VirtualCtap2Device::DeviceTransact(
    std::vector<uint8_t> command,
    DeviceCallback cb) {
  if (command.empty()) {
    ReturnCtap2Response(std::move(cb), CtapDeviceResponseCode::kCtap2ErrOther);
    return 0;
  }

  auto cmd_type = command[0];
  // The CTAP2 commands start at one, so a "command" of zero indicates that this
  // is a U2F message.
  if (cmd_type == 0 && config_.u2f_support) {
    u2f_device_->DeviceTransact(std::move(command), std::move(cb));
    return 0;
  }

  const auto request_bytes = base::make_span(command).subspan(1);
  CtapDeviceResponseCode response_code = CtapDeviceResponseCode::kCtap2ErrOther;
  std::vector<uint8_t> response_data;

  switch (static_cast<CtapRequestCommand>(cmd_type)) {
    case CtapRequestCommand::kAuthenticatorGetInfo:
      if (!request_bytes.empty()) {
        ReturnCtap2Response(std::move(cb),
                            CtapDeviceResponseCode::kCtap2ErrOther);
        return 0;
      }

      response_code = OnAuthenticatorGetInfo(&response_data);
      break;
    case CtapRequestCommand::kAuthenticatorMakeCredential: {
      auto opt_response_code = OnMakeCredential(request_bytes, &response_data);
      if (!opt_response_code) {
        // Simulate timeout due to unresponded User Presence check.
        return 0;
      }
      response_code = *opt_response_code;
      break;
    }
    case CtapRequestCommand::kAuthenticatorGetAssertion: {
      auto opt_response_code = OnGetAssertion(request_bytes, &response_data);
      if (!opt_response_code) {
        // Simulate timeout due to unresponded User Presence check.
        return 0;
      }
      response_code = *opt_response_code;
      break;
    }
    case CtapRequestCommand::kAuthenticatorGetNextAssertion:
      response_code = OnGetNextAssertion(request_bytes, &response_data);
      break;
    case CtapRequestCommand::kAuthenticatorClientPin: {
      auto opt_response_code = OnPINCommand(request_bytes, &response_data);
      if (!opt_response_code) {
        // Simulate timeout due to unresponded User Presence check.
        return 0;
      }
      response_code = *opt_response_code;
      break;
    }
    case CtapRequestCommand::kAuthenticatorCredentialManagement:
      response_code = OnCredentialManagement(request_bytes, &response_data);
      break;
    case CtapRequestCommand::kAuthenticatorBioEnrollment:
    case CtapRequestCommand::kAuthenticatorBioEnrollmentPreview:
      response_code = OnBioEnrollment(request_bytes, &response_data);
      break;
    default:
      break;
  }

  // Call |callback| via the |MessageLoop| because |AuthenticatorImpl| doesn't
  // support callback hairpinning.
  ReturnCtap2Response(std::move(cb), response_code, std::move(response_data));
  return 0;
}

base::WeakPtr<FidoDevice> VirtualCtap2Device::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

base::Optional<CtapDeviceResponseCode>
VirtualCtap2Device::CheckUserVerification(
    bool is_make_credential,
    const AuthenticatorSupportedOptions& options,
    const base::Optional<std::vector<uint8_t>>& pin_auth,
    const base::Optional<uint8_t>& pin_protocol,
    base::span<const uint8_t> pin_token,
    base::span<const uint8_t> client_data_hash,
    UserVerificationRequirement user_verification,
    bool* out_user_verified) {
  // The following quotes are from the CTAP2 spec:

  // 1. "If authenticator supports clientPin and platform sends a zero length
  // pinAuth, wait for user touch and then return either CTAP2_ERR_PIN_NOT_SET
  // if pin is not set or CTAP2_ERR_PIN_INVALID if pin has been set."
  const bool supports_pin =
      options.client_pin_availability !=
      AuthenticatorSupportedOptions::ClientPinAvailability::kNotSupported;
  if (supports_pin && pin_auth && pin_auth->empty()) {
    if (!SimulatePress())
      return base::nullopt;

    switch (options.client_pin_availability) {
      case AuthenticatorSupportedOptions::ClientPinAvailability::
          kSupportedAndPinSet:
        return CtapDeviceResponseCode::kCtap2ErrPinInvalid;
      case AuthenticatorSupportedOptions::ClientPinAvailability::
          kSupportedButPinNotSet:
        return CtapDeviceResponseCode::kCtap2ErrPinNotSet;
      case AuthenticatorSupportedOptions::ClientPinAvailability::kNotSupported:
        NOTREACHED();
    }
  }

  // 2. "If authenticator supports clientPin and pinAuth parameter is present
  // and the pinProtocol is not supported, return CTAP2_ERR_PIN_AUTH_INVALID
  // error."
  if (supports_pin && pin_auth && (!pin_protocol || *pin_protocol != 1)) {
    return CtapDeviceResponseCode::kCtap2ErrPinAuthInvalid;
  }

  // 3. "If authenticator is not protected by some form of user verification and
  // platform has set "uv" or pinAuth to get the user verification, return
  // CTAP2_ERR_INVALID_OPTION."
  const bool can_do_uv =
      options.user_verification_availability ==
          AuthenticatorSupportedOptions::UserVerificationAvailability::
              kSupportedAndConfigured ||
      options.client_pin_availability ==
          AuthenticatorSupportedOptions::ClientPinAvailability::
              kSupportedAndPinSet;
  if (!can_do_uv &&
      (user_verification == UserVerificationRequirement::kRequired ||
       pin_auth)) {
    return CtapDeviceResponseCode::kCtap2ErrInvalidOption;
  }

  // Step 4.
  bool uv = false;
  if (can_do_uv) {
    if (user_verification == UserVerificationRequirement::kRequired) {
      if (options.user_verification_availability ==
          AuthenticatorSupportedOptions::UserVerificationAvailability::
              kSupportedAndConfigured) {
        if (!SimulatePress())
          return base::nullopt;

        if (!config_.user_verification_succeeds) {
          if (is_make_credential)
            return CtapDeviceResponseCode::kCtap2ErrPinAuthInvalid;
          return CtapDeviceResponseCode::kCtap2ErrOperationDenied;
        }
        uv = true;
      } else {
        // UV was requested, but either not supported or not configured.
        return CtapDeviceResponseCode::kCtap2ErrPinAuthInvalid;
      }
    }

    if (pin_auth && (options.client_pin_availability ==
                         AuthenticatorSupportedOptions::ClientPinAvailability::
                             kSupportedAndPinSet ||
                     options.supports_uv_token)) {
      DCHECK(pin_protocol && *pin_protocol == 1);
      if (CheckPINToken(pin_token, *pin_auth, client_data_hash)) {
        uv = true;
      } else {
        return CtapDeviceResponseCode::kCtap2ErrPinAuthInvalid;
      }
    }

    if (is_make_credential && !uv) {
      return CtapDeviceResponseCode::kCtap2ErrPinRequired;
    }
  }

  *out_user_verified = uv;
  return CtapDeviceResponseCode::kSuccess;
}

base::Optional<CtapDeviceResponseCode> VirtualCtap2Device::OnMakeCredential(
    base::span<const uint8_t> request_bytes,
    std::vector<uint8_t>* response) {
  const auto& cbor_request = cbor::Reader::Read(request_bytes);
  if (!cbor_request || !cbor_request->is_map()) {
    DLOG(ERROR) << "Incorrectly formatted MakeCredential request.";
    return CtapDeviceResponseCode::kCtap2ErrOther;
  }

  CtapMakeCredentialRequest::ParseOpts parse_opts;
  parse_opts.reject_all_extensions = config_.reject_all_extensions;
  auto opt_request =
      CtapMakeCredentialRequest::Parse(cbor_request->GetMap(), parse_opts);
  if (!opt_request) {
    DLOG(ERROR) << "Incorrectly formatted MakeCredential request.";
    return CtapDeviceResponseCode::kCtap2ErrOther;
  }
  CtapMakeCredentialRequest request = std::move(*opt_request);
  const AuthenticatorSupportedOptions& options = device_info_->options;

  bool user_verified;
  const base::Optional<CtapDeviceResponseCode> uv_error = CheckUserVerification(
      true /* is makeCredential */, options, request.pin_auth,
      request.pin_protocol, mutable_state()->pin_token,
      request.client_data_hash, request.user_verification, &user_verified);
  if (uv_error != CtapDeviceResponseCode::kSuccess) {
    return uv_error;
  }

  // 6. Check for already registered credentials.
  const auto rp_id_hash = fido_parsing_utils::CreateSHA256Hash(request.rp.id);
  if ((config_.reject_large_allow_and_exclude_lists &&
       request.exclude_list.size() > 1) ||
      (config_.max_credential_count_in_list &&
       request.exclude_list.size() > config_.max_credential_count_in_list)) {
    return CtapDeviceResponseCode::kCtap2ErrLimitExceeded;
  }

  for (const auto& excluded_credential : request.exclude_list) {
    if (0 < config_.max_credential_id_length &&
        config_.max_credential_id_length < excluded_credential.id().size()) {
      return CtapDeviceResponseCode::kCtap2ErrLimitExceeded;
    }
    const RegistrationData* found =
        FindRegistrationData(excluded_credential.id(), rp_id_hash);
    if (found) {
      if (found->protection == device::CredProtect::kUVRequired &&
          !user_verified) {
        // Cannot disclose the existence of this credential without UV. If
        // a credentials ends up being created it'll overwrite this one.
        continue;
      }
      if (!SimulatePress()) {
        return base::nullopt;
      }
      return CtapDeviceResponseCode::kCtap2ErrCredentialExcluded;
    }
  }

  // Step 7.
  std::unique_ptr<PrivateKey> private_key;
  for (const auto& param :
       request.public_key_credential_params.public_key_credential_params()) {
    switch (param.algorithm) {
      default:
        continue;
      case static_cast<int32_t>(CoseAlgorithmIdentifier::kCoseEs256):
        private_key = FreshP256Key();
        break;
      case static_cast<int32_t>(CoseAlgorithmIdentifier::kCoseRs256):
        private_key = FreshRSAKey();
        break;
    }
    break;
  }

  if (!private_key) {
    DLOG(ERROR) << "Virtual CTAP2 device does not support any public-key "
                   "algorithm listed in the request";
    return CtapDeviceResponseCode::kCtap2ErrUnsupportedAlgorithm;
  }
  std::unique_ptr<PublicKey> public_key(private_key->GetPublicKey());

  // Step 8.
  if ((request.resident_key_required && !options.supports_resident_key) ||
      !options.supports_user_presence) {
    return CtapDeviceResponseCode::kCtap2ErrUnsupportedOption;
  }

  // Step 10.
  if (!user_verified && !SimulatePress()) {
    return base::nullopt;
  }

  // Our key handles are simple hashes of the public key.
  const auto key_handle = crypto::SHA256Hash(public_key->cose_key_bytes());

  base::Optional<cbor::Value> extensions;
  cbor::Value::MapValue extensions_map;
  if (request.hmac_secret) {
    extensions_map.emplace(cbor::Value(kExtensionHmacSecret),
                           cbor::Value(true));
  }

  CredProtect cred_protect = config_.default_cred_protect;
  if (request.cred_protect) {
    cred_protect = *request.cred_protect;
  }
  if (config_.force_cred_protect) {
    cred_protect = *config_.force_cred_protect;
  }

  if (request.cred_protect ||
      cred_protect != device::CredProtect::kUVOptional) {
    extensions_map.emplace(cbor::Value(kExtensionCredProtect),
                           cbor::Value(static_cast<int64_t>(cred_protect)));
  }

  if (config_.add_extra_extension) {
    extensions_map.emplace(cbor::Value("unsolicited"), cbor::Value(42));
  }

  if (!extensions_map.empty()) {
    extensions = cbor::Value(std::move(extensions_map));
  }

  auto authenticator_data = ConstructAuthenticatorData(
      rp_id_hash, user_verified, 01ul,
      ConstructAttestedCredentialData(key_handle, std::move(public_key)),
      std::move(extensions));

  base::Optional<std::string> opt_android_client_data_json;
  if (request.android_client_data_ext &&
      config_.support_android_client_data_extension) {
    opt_android_client_data_json.emplace(ConstructAndroidClientDataJSON(
        *request.android_client_data_ext, "webauthn.create"));
  }

  auto sign_buffer = ConstructSignatureBuffer(
      authenticator_data,
      opt_android_client_data_json
          ? fido_parsing_utils::CreateSHA256Hash(*opt_android_client_data_json)
          : request.client_data_hash);

  // Sign with attestation key.
  // Note: Non-deterministic, you need to mock this out if you rely on
  // deterministic behavior.
  std::vector<uint8_t> sig;
  std::unique_ptr<crypto::ECPrivateKey> attestation_private_key =
      crypto::ECPrivateKey::CreateFromPrivateKeyInfo(GetAttestationKey());
  bool status =
      Sign(attestation_private_key.get(), std::move(sign_buffer), &sig);
  DCHECK(status);

  base::Optional<std::vector<uint8_t>> attestation_cert;
  if (!mutable_state()->self_attestation) {
    attestation_cert = GenerateAttestationCertificate(
        false /* individual_attestation_requested */);
    if (!attestation_cert) {
      DLOG(ERROR) << "Failed to generate attestation certificate.";
      return CtapDeviceResponseCode::kCtap2ErrOther;
    }
  }

  base::Optional<std::vector<uint8_t>> opt_android_client_data_ext;
  if (opt_android_client_data_json) {
    opt_android_client_data_ext.emplace();
    fido_parsing_utils::Append(
        &*opt_android_client_data_ext,
        base::make_span(reinterpret_cast<const uint8_t*>(
                            opt_android_client_data_json->data()),
                        opt_android_client_data_json->size()));
  } else if (config_.send_unsolicited_android_client_data_extension) {
    const std::string client_data_json =
        "{\"challenge\":"
        "\"ZXlKaGJHY2lPaUpJVXpJMU5pSXNJblI1Y0NJNklrcFhWQ0o5LmV5SnBZWFFpT2pFMU"
        "9EYzNOamMxTnpJc0ltVjRjQ0k2TVRVNE56ZzROelUzTWl3aWMzVmlJam9pWkdaa1ptY2"
        "lmUS5FdFFyUXNSWE9qNlpkMGFseXVkUzF3X3FORjJSbElZdTNfb0NvTDRzbWI4\","
        "\"origin\":" +
        base::GetQuotedJSONString("https://" + request.rp.id) +
        ",\"type\":\"webauthn.create\",\"androidPackageName\":\"org.chromium."
        "device.fido.test\"}";
    opt_android_client_data_ext.emplace();
    fido_parsing_utils::Append(&*opt_android_client_data_ext,
                               base::make_span(reinterpret_cast<const uint8_t*>(
                                                   client_data_json.data()),
                                               client_data_json.size()));
  }

  *response = ConstructMakeCredentialResponse(
      std::move(attestation_cert), sig, std::move(authenticator_data),
      std::move(opt_android_client_data_ext));
  RegistrationData registration(std::move(private_key), rp_id_hash,
                                1 /* signature counter */);

  if (request.resident_key_required) {
    // If there's already a registration for this RP and user ID, delete it.
    for (const auto& registration : mutable_state()->registrations) {
      if (registration.second.is_resident &&
          rp_id_hash == registration.second.application_parameter &&
          registration.second.user->id == request.user.id) {
        mutable_state()->registrations.erase(registration.first);
        break;
      }
    }

    size_t num_resident_keys = 0;
    for (const auto& registration : mutable_state()->registrations) {
      if (registration.second.is_resident) {
        num_resident_keys++;
      }
    }

    if (num_resident_keys >= config_.resident_credential_storage) {
      return CtapDeviceResponseCode::kCtap2ErrKeyStoreFull;
    }

    registration.is_resident = true;
    registration.user = request.user;
    registration.rp = request.rp;
  }

  registration.protection = cred_protect;
  StoreNewKey(key_handle, std::move(registration));
  return CtapDeviceResponseCode::kSuccess;
}

base::Optional<CtapDeviceResponseCode> VirtualCtap2Device::OnGetAssertion(
    base::span<const uint8_t> request_bytes,
    std::vector<uint8_t>* response) {
  // Step numbers in this function refer to
  // https://fidoalliance.org/specs/fido-v2.0-ps-20190130/fido-client-to-authenticator-protocol-v2.0-ps-20190130.html#authenticatorGetAssertion
  const auto& cbor_request = cbor::Reader::Read(request_bytes);
  if (!cbor_request || !cbor_request->is_map()) {
    DLOG(ERROR) << "Incorrectly formatted MakeCredential request.";
    return CtapDeviceResponseCode::kCtap2ErrOther;
  }

  const auto& request_map = cbor_request->GetMap();
  CtapGetAssertionRequest::ParseOpts parse_opts;
  parse_opts.reject_all_extensions = config_.reject_all_extensions;
  auto opt_request = CtapGetAssertionRequest::Parse(request_map, parse_opts);
  if (!opt_request) {
    DLOG(ERROR) << "Incorrectly formatted GetAssertion request.";
    return CtapDeviceResponseCode::kCtap2ErrOther;
  }
  CtapGetAssertionRequest request = std::move(*opt_request);
  const AuthenticatorSupportedOptions& options = device_info_->options;

  bool user_verified;
  const base::Optional<CtapDeviceResponseCode> uv_error = CheckUserVerification(
      false /* not makeCredential */, options, request.pin_auth,
      request.pin_protocol, mutable_state()->pin_token,
      request.client_data_hash, request.user_verification, &user_verified);
  if (uv_error != CtapDeviceResponseCode::kSuccess) {
    return uv_error;
  }

  if (!config_.resident_key_support && request.allow_list.empty()) {
    return CtapDeviceResponseCode::kCtap2ErrNoCredentials;
  }

  const auto rp_id_hash = fido_parsing_utils::CreateSHA256Hash(request.rp_id);

  std::vector<std::pair<base::span<const uint8_t>, RegistrationData*>>
      found_registrations;

  if (!request.user_presence_required &&
      config_.reject_silent_authentication_requests) {
    return CtapDeviceResponseCode::kCtap2ErrUnsupportedOption;
  }

  if ((config_.reject_large_allow_and_exclude_lists &&
       request.allow_list.size() > 1) ||
      (config_.max_credential_count_in_list &&
       request.allow_list.size() > config_.max_credential_count_in_list)) {
    return CtapDeviceResponseCode::kCtap2ErrLimitExceeded;
  }

  // An empty allow_list could be considered to be a resident-key request, but
  // some authenticators in practice don't take it that way. Thus this code
  // mirrors that to better reflect reality. CTAP 2.0 leaves it as undefined
  // behaviour.
  for (const auto& allowed_credential : request.allow_list) {
    if (0 < config_.max_credential_id_length &&
        config_.max_credential_id_length < allowed_credential.id().size()) {
      return CtapDeviceResponseCode::kCtap2ErrLimitExceeded;
    }
    RegistrationData* found =
        FindRegistrationData(allowed_credential.id(), rp_id_hash);
    if (found) {
      found_registrations.emplace_back(allowed_credential.id(), found);
      break;
    }
  }
  const auto allow_list_it = request_map.find(cbor::Value(3));
  if (allow_list_it == request_map.end()) {
    DCHECK(config_.resident_key_support);
    for (auto& registration : mutable_state()->registrations) {
      if (registration.second.is_resident &&
          registration.second.application_parameter == rp_id_hash) {
        found_registrations.emplace_back(registration.first,
                                         &registration.second);
      }
    }
  }

  // Enforce credProtect semantics.
  found_registrations.erase(
      std::remove_if(
          found_registrations.begin(), found_registrations.end(),
          [user_verified, &request](
              const std::pair<base::span<const uint8_t>, RegistrationData*>&
                  candidate) -> bool {
            switch (candidate.second->protection) {
              case CredProtect::kUVOptional:
                return false;
              case CredProtect::kUVOrCredIDRequired:
                return request.allow_list.empty() && !user_verified;
              case CredProtect::kUVRequired:
                return !user_verified;
            }
          }),
      found_registrations.end());

  if (config_.return_immediate_invalid_credential_error &&
      found_registrations.empty()) {
    return CtapDeviceResponseCode::kCtap2ErrInvalidCredential;
  }

  // Step 5.
  if (!options.supports_user_presence && request.user_presence_required) {
    return CtapDeviceResponseCode::kCtap2ErrUnsupportedOption;
  }

  // Step 7.
  if (request.user_presence_required && !user_verified && !SimulatePress())
    return base::nullopt;

  // Step 8.
  if (found_registrations.empty()) {
    return CtapDeviceResponseCode::kCtap2ErrNoCredentials;
  }

  base::Optional<cbor::Value> extensions;
  cbor::Value::MapValue extensions_map;
  if (config_.add_extra_extension) {
    extensions_map.emplace(cbor::Value("unsolicited"), cbor::Value(42));
  }
  if (!extensions_map.empty()) {
    extensions = cbor::Value(std::move(extensions_map));
  }

  // This implementation does not sort credentials by creation time as the spec
  // requires.

  mutable_state()->pending_assertions.clear();
  bool done_first = false;
  for (const auto& registration : found_registrations) {
    registration.second->counter++;

    base::Optional<AttestedCredentialData> opt_attested_cred_data;
    if (config_.return_attested_cred_data_in_get_assertion_response) {
      opt_attested_cred_data.emplace(ConstructAttestedCredentialData(
          registration.first,
          registration.second->private_key->GetPublicKey()));
    }

    auto authenticator_data = ConstructAuthenticatorData(
        rp_id_hash, user_verified, registration.second->counter,
        std::move(opt_attested_cred_data),
        extensions ? base::make_optional(extensions->Clone()) : base::nullopt);

    base::Optional<std::string> opt_android_client_data_json;
    if (request.android_client_data_ext &&
        config_.support_android_client_data_extension) {
      opt_android_client_data_json.emplace(ConstructAndroidClientDataJSON(
          *request.android_client_data_ext, "webauthn.get"));
    }

    auto signature_buffer = ConstructSignatureBuffer(
        authenticator_data, opt_android_client_data_json
                                ? fido_parsing_utils::CreateSHA256Hash(
                                      *opt_android_client_data_json)
                                : request.client_data_hash);

    std::vector<uint8_t> signature =
        registration.second->private_key->Sign(signature_buffer);

    AuthenticatorGetAssertionResponse assertion(
        std::move(authenticator_data),
        fido_parsing_utils::Materialize(signature));

    assertion.SetCredential(
        {CredentialType::kPublicKey,
         fido_parsing_utils::Materialize(registration.first)});
    if (registration.second->is_resident) {
      assertion.SetUserEntity(registration.second->user.value());
    }

    if (opt_android_client_data_json) {
      std::vector<uint8_t> android_client_data_ext;
      fido_parsing_utils::Append(
          &android_client_data_ext,
          base::make_span(reinterpret_cast<const uint8_t*>(
                              opt_android_client_data_json->data()),
                          opt_android_client_data_json->size()));
      assertion.set_android_client_data_ext(std::move(android_client_data_ext));
    } else if (config_.send_unsolicited_android_client_data_extension) {
      const std::string client_data_json =
          "{challenge:"
          "\"ZXlKaGJHY2lPaUpJVXpJMU5pSXNJblI1Y0NJNklrcFhWQ0o5LmV5SnBZWFFpT2pFMU"
          "9EYzNOamMxTnpJc0ltVjRjQ0k2TVRVNE56ZzROelUzTWl3aWMzVmlJam9pWkdaa1ptY2"
          "lmUS5FdFFyUXNSWE9qNlpkMGFseXVkUzF3X3FORjJSbElZdTNfb0NvTDRzbWI4\","
          "origin:\"https://" +
          request.rp_id + "\",type:\"webauthn.get\"}";
      std::vector<uint8_t> android_client_data_ext;
      fido_parsing_utils::Append(
          &android_client_data_ext,
          base::make_span(
              reinterpret_cast<const uint8_t*>(client_data_json.data()),
              client_data_json.size()));
      assertion.set_android_client_data_ext(std::move(android_client_data_ext));
    }

    if (!done_first) {
      if (found_registrations.size() > 1) {
        DCHECK_LT(found_registrations.size(), 256u);
        assertion.SetNumCredentials(found_registrations.size());
      }
      *response = EncodeGetAssertionResponse(
          assertion, config_.allow_invalid_utf8_in_credential_entities);
      done_first = true;
    } else {
      // These replies will be returned in response to a GetNextAssertion
      // request.
      mutable_state()->pending_assertions.emplace_back(
          EncodeGetAssertionResponse(
              assertion, config_.allow_invalid_utf8_in_credential_entities));
    }
  }

  return CtapDeviceResponseCode::kSuccess;
}

CtapDeviceResponseCode VirtualCtap2Device::OnGetNextAssertion(
    base::span<const uint8_t> request_bytes,
    std::vector<uint8_t>* response) {
  if (!request_bytes.empty() && !cbor::Reader::Read(request_bytes)) {
    return CtapDeviceResponseCode::kCtap2ErrCBORUnexpectedType;
  }

  auto& pending_assertions = mutable_state()->pending_assertions;
  if (pending_assertions.empty()) {
    return CtapDeviceResponseCode::kCtap2ErrNotAllowed;
  }

  *response = std::move(pending_assertions.back());
  pending_assertions.pop_back();

  return CtapDeviceResponseCode::kSuccess;
}

base::Optional<CtapDeviceResponseCode> VirtualCtap2Device::OnPINCommand(
    base::span<const uint8_t> request_bytes,
    std::vector<uint8_t>* response) {
  if (device_info_->options.client_pin_availability ==
          AuthenticatorSupportedOptions::ClientPinAvailability::kNotSupported &&
      !config_.uv_token_support) {
    return CtapDeviceResponseCode::kCtap1ErrInvalidCommand;
  }

  const auto& cbor_request = cbor::Reader::Read(request_bytes);
  if (!cbor_request || !cbor_request->is_map()) {
    return CtapDeviceResponseCode::kCtap2ErrCBORUnexpectedType;
  }
  const auto& request_map = cbor_request->GetMap();

  const auto protocol_it = request_map.find(
      cbor::Value(static_cast<int>(pin::RequestKey::kProtocol)));
  if (protocol_it == request_map.end() || !protocol_it->second.is_unsigned()) {
    return CtapDeviceResponseCode::kCtap2ErrCBORUnexpectedType;
  }
  if (protocol_it->second.GetUnsigned() != pin::kProtocolVersion) {
    return CtapDeviceResponseCode::kCtap1ErrInvalidCommand;
  }

  const auto subcommand_it = request_map.find(
      cbor::Value(static_cast<int>(pin::RequestKey::kSubcommand)));
  if (subcommand_it == request_map.end() ||
      !subcommand_it->second.is_unsigned()) {
    return CtapDeviceResponseCode::kCtap2ErrCBORUnexpectedType;
  }
  const int64_t subcommand = subcommand_it->second.GetUnsigned();

  cbor::Value::MapValue response_map;
  switch (subcommand) {
    case static_cast<int>(device::pin::Subcommand::kGetRetries):
      response_map.emplace(static_cast<int>(pin::ResponseKey::kRetries),
                           mutable_state()->pin_retries);
      break;

    case static_cast<int>(device::pin::Subcommand::kGetUvRetries):
      response_map.emplace(static_cast<int>(pin::ResponseKey::kUvRetries),
                           mutable_state()->uv_retries);
      break;

    case static_cast<int>(device::pin::Subcommand::kGetKeyAgreement): {
      bssl::UniquePtr<EC_KEY> key(
          EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
      CHECK(EC_KEY_generate_key(key.get()));
      response_map.emplace(static_cast<int>(pin::ResponseKey::kKeyAgreement),
                           pin::EncodeCOSEPublicKey(key.get()));
      mutable_state()->ecdh_key = std::move(key);
      break;
    }

    case static_cast<int>(device::pin::Subcommand::kSetPIN): {
      const auto encrypted_pin =
          GetPINBytestring(request_map, pin::RequestKey::kNewPINEnc);
      const auto pin_auth =
          GetPINBytestring(request_map, pin::RequestKey::kPINAuth);
      const auto peer_key =
          GetPINKey(request_map, pin::RequestKey::kKeyAgreement);

      if (!encrypted_pin || (encrypted_pin->size() % AES_BLOCK_SIZE) != 0 ||
          !pin_auth || !peer_key) {
        return CtapDeviceResponseCode::kCtap2ErrMissingParameter;
      }

      if (!mutable_state()->pin.empty()) {
        return CtapDeviceResponseCode::kCtap2ErrPinAuthInvalid;
      }

      uint8_t shared_key[SHA256_DIGEST_LENGTH];
      if (!mutable_state()->ecdh_key) {
        // kGetKeyAgreement should have been called first.
        NOTREACHED();
        return CtapDeviceResponseCode::kCtap2ErrPinTokenExpired;
      }
      pin::CalculateSharedKey(mutable_state()->ecdh_key.get(), peer_key->get(),
                              shared_key);

      CtapDeviceResponseCode err =
          SetPIN(mutable_state(), shared_key, *encrypted_pin, *pin_auth);
      if (err != CtapDeviceResponseCode::kSuccess) {
        return err;
      };

      AuthenticatorSupportedOptions options = device_info_->options;
      options.client_pin_availability = AuthenticatorSupportedOptions::
          ClientPinAvailability::kSupportedAndPinSet;
      device_info_->options = std::move(options);

      break;
    }

    case static_cast<int>(device::pin::Subcommand::kChangePIN): {
      const auto encrypted_new_pin =
          GetPINBytestring(request_map, pin::RequestKey::kNewPINEnc);
      const auto encrypted_pin_hash =
          GetPINBytestring(request_map, pin::RequestKey::kPINHashEnc);
      const auto pin_auth =
          GetPINBytestring(request_map, pin::RequestKey::kPINAuth);
      const auto peer_key =
          GetPINKey(request_map, pin::RequestKey::kKeyAgreement);

      if (!encrypted_pin_hash || encrypted_pin_hash->size() != AES_BLOCK_SIZE ||
          !encrypted_new_pin ||
          (encrypted_new_pin->size() % AES_BLOCK_SIZE) != 0 || !pin_auth ||
          !peer_key) {
        return CtapDeviceResponseCode::kCtap2ErrMissingParameter;
      }

      uint8_t shared_key[SHA256_DIGEST_LENGTH];
      if (!mutable_state()->ecdh_key) {
        // kGetKeyAgreement should have been called first.
        NOTREACHED();
        return CtapDeviceResponseCode::kCtap2ErrPinTokenExpired;
      }
      pin::CalculateSharedKey(mutable_state()->ecdh_key.get(), peer_key->get(),
                              shared_key);

      CtapDeviceResponseCode err =
          ConfirmPresentedPIN(mutable_state(), shared_key, *encrypted_pin_hash);
      if (err != CtapDeviceResponseCode::kSuccess) {
        return err;
      };

      err = SetPIN(mutable_state(), shared_key, *encrypted_new_pin, *pin_auth);
      if (err != CtapDeviceResponseCode::kSuccess) {
        return err;
      };

      break;
    }

    case static_cast<int>(device::pin::Subcommand::kGetPINToken): {
      const auto encrypted_pin_hash =
          GetPINBytestring(request_map, pin::RequestKey::kPINHashEnc);
      const auto peer_key =
          GetPINKey(request_map, pin::RequestKey::kKeyAgreement);

      if (!encrypted_pin_hash || encrypted_pin_hash->size() != AES_BLOCK_SIZE ||
          !peer_key) {
        return CtapDeviceResponseCode::kCtap2ErrMissingParameter;
      }

      uint8_t shared_key[SHA256_DIGEST_LENGTH];
      if (!mutable_state()->ecdh_key) {
        // kGetKeyAgreement should have been called first.
        NOTREACHED();
        return CtapDeviceResponseCode::kCtap2ErrPinTokenExpired;
      }
      pin::CalculateSharedKey(mutable_state()->ecdh_key.get(), peer_key->get(),
                              shared_key);

      CtapDeviceResponseCode err =
          ConfirmPresentedPIN(mutable_state(), shared_key, *encrypted_pin_hash);
      if (err != CtapDeviceResponseCode::kSuccess) {
        return err;
      };

      response_map.emplace(
          static_cast<int>(pin::ResponseKey::kPINToken),
          GenerateAndEncryptToken(shared_key, mutable_state()->pin_token));
      break;
    }

    case static_cast<int>(device::pin::Subcommand::kGetUvToken): {
      const auto peer_key =
          GetPINKey(request_map, pin::RequestKey::kKeyAgreement);
      if (!peer_key) {
        return CtapDeviceResponseCode::kCtap2ErrMissingParameter;
      }

      if (device_info_->options.user_verification_availability ==
          AuthenticatorSupportedOptions::UserVerificationAvailability::
              kSupportedButNotConfigured) {
        return CtapDeviceResponseCode::kCtap2ErrNotAllowed;
      }

      if (mutable_state()->uv_retries <= 0) {
        return CtapDeviceResponseCode::kCtap2ErrUvBlocked;
      }

      uint8_t shared_key[SHA256_DIGEST_LENGTH];
      if (!mutable_state()->ecdh_key) {
        // kGetKeyAgreement should have been called first.
        NOTREACHED();
        return CtapDeviceResponseCode::kCtap2ErrPinTokenExpired;
      }
      pin::CalculateSharedKey(mutable_state()->ecdh_key.get(), peer_key->get(),
                              shared_key);

      --mutable_state()->uv_retries;

      // Simulate internal UV.
      if (!SimulatePress()) {
        return base::nullopt;
      }
      if (!config_.user_verification_succeeds) {
        return CtapDeviceResponseCode::kCtap2ErrPinInvalid;
      }

      mutable_state()->pin_retries = kMaxPinRetries;
      mutable_state()->uv_retries = kMaxUvRetries;

      response_map.emplace(
          static_cast<int>(pin::ResponseKey::kPINToken),
          GenerateAndEncryptToken(shared_key, mutable_state()->pin_token));
      break;
    }

    default:
      return CtapDeviceResponseCode::kCtap1ErrInvalidCommand;
  }

  *response = cbor::Writer::Write(cbor::Value(std::move(response_map))).value();
  return CtapDeviceResponseCode::kSuccess;
}

CtapDeviceResponseCode VirtualCtap2Device::OnCredentialManagement(
    base::span<const uint8_t> request_bytes,
    std::vector<uint8_t>* response) {
  if (!device_info_->options.supports_credential_management) {
    return CtapDeviceResponseCode::kCtap2ErrUnsupportedOption;
  }

  const auto& cbor_request = cbor::Reader::Read(request_bytes);
  if (!cbor_request || !cbor_request->is_map()) {
    return CtapDeviceResponseCode::kCtap2ErrCBORUnexpectedType;
  }
  const auto& request_map = cbor_request->GetMap();
  const auto subcommand_it = request_map.find(cbor::Value(
      static_cast<int>(CredentialManagementRequestKey::kSubCommand)));
  if (subcommand_it == request_map.end() ||
      !subcommand_it->second.is_unsigned()) {
    return CtapDeviceResponseCode::kCtap2ErrCBORUnexpectedType;
  }
  const int64_t subcommand = subcommand_it->second.GetUnsigned();

  cbor::Value::MapValue response_map;
  switch (static_cast<CredentialManagementSubCommand>(subcommand)) {
    case CredentialManagementSubCommand::kGetCredsMetadata: {
      CtapDeviceResponseCode pin_status = CheckCredentialManagementPINAuth(
          request_map, mutable_state()->pin_token,
          {{static_cast<uint8_t>(subcommand)}});
      if (pin_status != CtapDeviceResponseCode::kSuccess) {
        return pin_status;
      }

      const size_t num_resident =
          std::count_if(mutable_state()->registrations.begin(),
                        mutable_state()->registrations.end(),
                        [](const auto& it) { return it.second.is_resident; });
      response_map.emplace(
          static_cast<int>(CredentialManagementResponseKey::
                               kExistingResidentCredentialsCount),
          static_cast<int64_t>(num_resident));

      const size_t num_remaining =
          config_.resident_credential_storage - num_resident;
      DCHECK(0 <= num_remaining);
      response_map.emplace(
          static_cast<int>(CredentialManagementResponseKey::
                               kMaxPossibleRemainingResidentCredentialsCount),
          static_cast<int64_t>(num_remaining));

      *response =
          cbor::Writer::Write(cbor::Value(std::move(response_map))).value();
      return CtapDeviceResponseCode::kSuccess;
    }

    case CredentialManagementSubCommand::kEnumerateRPsBegin: {
      CtapDeviceResponseCode pin_status = CheckCredentialManagementPINAuth(
          request_map, mutable_state()->pin_token,
          {{static_cast<uint8_t>(subcommand)}});
      if (pin_status != CtapDeviceResponseCode::kSuccess) {
        return pin_status;
      }

      InitPendingRPs();
      response_map.emplace(
          static_cast<int>(CredentialManagementResponseKey::kTotalRPs),
          static_cast<int>(mutable_state()->pending_rps.size()));
      if (!mutable_state()->pending_rps.empty()) {
        GetNextRP(&response_map);
      }

      *response = WriteCBOR(cbor::Value(std::move(response_map)),
                            config_.allow_invalid_utf8_in_credential_entities);
      return CtapDeviceResponseCode::kSuccess;
    }

    case CredentialManagementSubCommand::kEnumerateRPsGetNextRP: {
      if (mutable_state()->pending_rps.empty()) {
        return CtapDeviceResponseCode::kCtap2ErrNotAllowed;
      }
      GetNextRP(&response_map);

      *response = WriteCBOR(cbor::Value(std::move(response_map)),
                            config_.allow_invalid_utf8_in_credential_entities);
      return CtapDeviceResponseCode::kSuccess;
    }

    case CredentialManagementSubCommand::kEnumerateCredentialsBegin: {
      const auto params_it = request_map.find(cbor::Value(
          static_cast<int>(CredentialManagementRequestKey::kSubCommandParams)));
      if (params_it == request_map.end() && !params_it->second.is_map()) {
        return CtapDeviceResponseCode::kCtap2ErrCBORUnexpectedType;
      }
      const cbor::Value::MapValue& params = params_it->second.GetMap();

      // pinAuth = LEFT(HMAC-SHA-256(pinToken, enumerateCredentialsBegin (0x04)
      //                                       || subCommandParams), 16)
      std::vector<uint8_t> pinauth_bytes =
          cbor::Writer::Write(cbor::Value(params)).value();
      pinauth_bytes.insert(pinauth_bytes.begin(),
                           static_cast<uint8_t>(subcommand));
      CtapDeviceResponseCode pin_status = CheckCredentialManagementPINAuth(
          request_map, mutable_state()->pin_token, pinauth_bytes);
      if (pin_status != CtapDeviceResponseCode::kSuccess) {
        return pin_status;
      }

      const auto rp_id_hash_it = params.find(cbor::Value(
          static_cast<int>(CredentialManagementRequestParamKey::kRPIDHash)));
      if (rp_id_hash_it == params.end() ||
          !rp_id_hash_it->second.is_bytestring() ||
          rp_id_hash_it->second.GetBytestring().size() != kRpIdHashLength) {
        return CtapDeviceResponseCode::kCtap2ErrCBORUnexpectedType;
      }

      InitPendingRegistrations(rp_id_hash_it->second.GetBytestring());
      if (mutable_state()->pending_registrations.empty()) {
        return CtapDeviceResponseCode::kCtap2ErrNoCredentials;
      }
      response_map.swap(mutable_state()->pending_registrations.front());
      response_map.emplace(
          static_cast<int>(CredentialManagementResponseKey::kTotalCredentials),
          static_cast<int>(mutable_state()->pending_registrations.size()));
      mutable_state()->pending_registrations.pop_front();

      *response = WriteCBOR(cbor::Value(std::move(response_map)),
                            config_.allow_invalid_utf8_in_credential_entities);
      return CtapDeviceResponseCode::kSuccess;
    }

    case CredentialManagementSubCommand::
        kEnumerateCredentialsGetNextCredential: {
      if (mutable_state()->pending_registrations.empty()) {
        return CtapDeviceResponseCode::kCtap2ErrNotAllowed;
      }
      response_map.swap(mutable_state()->pending_registrations.front());
      mutable_state()->pending_registrations.pop_front();

      *response = WriteCBOR(cbor::Value(std::move(response_map)),
                            config_.allow_invalid_utf8_in_credential_entities);
      return CtapDeviceResponseCode::kSuccess;
    }

    case CredentialManagementSubCommand::kDeleteCredential: {
      const auto params_it = request_map.find(cbor::Value(
          static_cast<int>(CredentialManagementRequestKey::kSubCommandParams)));
      if (params_it == request_map.end() && !params_it->second.is_map()) {
        return CtapDeviceResponseCode::kCtap2ErrCBORUnexpectedType;
      }
      const cbor::Value::MapValue& params = params_it->second.GetMap();
      // pinAuth = LEFT(HMAC-SHA-256(pinToken, enumerateCredentialsBegin (0x04)
      //                                       || subCommandParams), 16)
      std::vector<uint8_t> pinauth_bytes =
          cbor::Writer::Write(cbor::Value(params)).value();
      pinauth_bytes.insert(pinauth_bytes.begin(),
                           static_cast<uint8_t>(subcommand));
      CtapDeviceResponseCode pin_status = CheckCredentialManagementPINAuth(
          request_map, mutable_state()->pin_token, pinauth_bytes);
      if (pin_status != CtapDeviceResponseCode::kSuccess) {
        return pin_status;
      }

      // The spec doesn't say, but we clear the enumerateRPs and
      // enumerateCredentials states after deleteCredential to avoid having to
      // update them.
      mutable_state()->pending_rps.clear();
      mutable_state()->pending_registrations.clear();

      const auto credential_id_it = params.find(cbor::Value(static_cast<int>(
          CredentialManagementRequestParamKey::kCredentialID)));
      if (credential_id_it == params.end() ||
          !credential_id_it->second.is_map()) {
        return CtapDeviceResponseCode::kCtap2ErrCBORUnexpectedType;
      }
      auto credential_id = PublicKeyCredentialDescriptor::CreateFromCBORValue(
          cbor::Value(credential_id_it->second.GetMap()));
      if (!credential_id) {
        return CtapDeviceResponseCode::kCtap2ErrCBORUnexpectedType;
      }
      if (!base::Contains(mutable_state()->registrations,
                          credential_id->id())) {
        return CtapDeviceResponseCode::kCtap2ErrNoCredentials;
      }
      mutable_state()->registrations.erase(credential_id->id());
      *response = {};
      return CtapDeviceResponseCode::kSuccess;
    }
  }
  NOTREACHED();
  return CtapDeviceResponseCode::kCtap2ErrInvalidOption;
}

CtapDeviceResponseCode VirtualCtap2Device::OnBioEnrollment(
    base::span<const uint8_t> request_bytes,
    std::vector<uint8_t>* response) {
  // Check to ensure that device supports bio enrollment.
  if (device_info_->options.bio_enrollment_availability ==
          AuthenticatorSupportedOptions::BioEnrollmentAvailability::
              kNotSupported &&
      device_info_->options.bio_enrollment_availability_preview ==
          AuthenticatorSupportedOptions::BioEnrollmentAvailability::
              kNotSupported) {
    return CtapDeviceResponseCode::kCtap2ErrUnsupportedOption;
  }

  // Read request bytes into |cbor::Value::MapValue|.
  const auto& cbor_request = cbor::Reader::Read(request_bytes);
  if (!cbor_request || !cbor_request->is_map()) {
    return CtapDeviceResponseCode::kCtap2ErrCBORUnexpectedType;
  }
  const auto& request_map = cbor_request->GetMap();

  cbor::Value::MapValue response_map;

  // Check for the get-modality command.
  auto it = request_map.find(
      cbor::Value(static_cast<int>(BioEnrollmentRequestKey::kGetModality)));
  if (it != request_map.end()) {
    if (!it->second.is_bool()) {
      return CtapDeviceResponseCode::kCtap2ErrCBORUnexpectedType;
    }
    if (!it->second.GetBool()) {
      // This value is optional so sending |false| is prohibited by the spec.
      return CtapDeviceResponseCode::kCtap2ErrInvalidOption;
    }
    response_map.emplace(static_cast<int>(BioEnrollmentResponseKey::kModality),
                         static_cast<int>(BioEnrollmentModality::kFingerprint));
    *response =
        cbor::Writer::Write(cbor::Value(std::move(response_map))).value();
    return CtapDeviceResponseCode::kSuccess;
  }

  // Check for subcommands.
  it = request_map.find(
      cbor::Value(static_cast<int>(BioEnrollmentRequestKey::kSubCommand)));
  if (it == request_map.end()) {
    // Could not find a valid command, so return an error.
    NOTREACHED();
    return CtapDeviceResponseCode::kCtap2ErrInvalidOption;
  }

  if (!it->second.is_unsigned()) {
    return CtapDeviceResponseCode::kCtap2ErrCBORUnexpectedType;
  }

  // Template id from subcommand parameters, if it exists.
  base::Optional<uint8_t> template_id;
  base::Optional<std::string> name;
  auto params_it = request_map.find(cbor::Value(
      static_cast<int>(BioEnrollmentRequestKey::kSubCommandParams)));
  if (params_it != request_map.end()) {
    const auto& params = params_it->second.GetMap();
    auto template_it = params.find(cbor::Value(
        static_cast<int>(BioEnrollmentSubCommandParam::kTemplateId)));
    if (template_it != params.end()) {
      if (!template_it->second.is_bytestring()) {
        NOTREACHED() << "Template ID parameter must be a CBOR bytestring.";
        return CtapDeviceResponseCode::kCtap2ErrCBORUnexpectedType;
      }
      // Simplification: for unit tests, enforce one byte template IDs
      DCHECK_EQ(template_it->second.GetBytestring().size(), 1u);
      template_id = template_it->second.GetBytestring()[0];
    }
    auto name_it = params.find(cbor::Value(
        static_cast<int>(BioEnrollmentSubCommandParam::kTemplateFriendlyName)));
    if (name_it != params.end()) {
      if (!name_it->second.is_string()) {
        NOTREACHED() << "Name parameter must be a CBOR string.";
        return CtapDeviceResponseCode::kCtap2ErrCBORUnexpectedType;
      }
      name = name_it->second.GetString();
    }
  }

  auto cmd =
      ToBioEnrollmentEnum<BioEnrollmentSubCommand>(it->second.GetUnsigned());
  if (!cmd) {
    // Invalid command is unsupported.
    return CtapDeviceResponseCode::kCtap2ErrUnsupportedOption;
  }

  using SubCmd = BioEnrollmentSubCommand;
  switch (*cmd) {
    case SubCmd::kGetFingerprintSensorInfo:
      response_map.emplace(
          static_cast<int>(BioEnrollmentResponseKey::kModality),
          static_cast<int>(BioEnrollmentModality::kFingerprint));
      response_map.emplace(
          static_cast<int>(BioEnrollmentResponseKey::kFingerprintKind),
          static_cast<int>(BioEnrollmentFingerprintKind::kTouch));
      response_map.emplace(
          static_cast<int>(
              BioEnrollmentResponseKey::kMaxCaptureSamplesRequiredForEnroll),
          config_.bio_enrollment_samples_required);
      break;
    case SubCmd::kEnrollBegin:
      if (mutable_state()->bio_templates.size() ==
          config_.bio_enrollment_capacity) {
        return CtapDeviceResponseCode::kCtap2ErrKeyStoreFull;
      }
      mutable_state()->bio_current_template_id = 0;
      while (mutable_state()->bio_templates.find(
                 ++(*mutable_state()->bio_current_template_id)) !=
             mutable_state()->bio_templates.end()) {
        // Check for integer overflow (indicates full)
        DCHECK(*mutable_state()->bio_current_template_id < 255);
      }
      mutable_state()->bio_remaining_samples =
          config_.bio_enrollment_samples_required;
      response_map.emplace(
          static_cast<int>(BioEnrollmentResponseKey::kTemplateId),
          std::vector<uint8_t>{*mutable_state()->bio_current_template_id});
      response_map.emplace(
          static_cast<int>(BioEnrollmentResponseKey::kLastEnrollSampleStatus),
          static_cast<int>(BioEnrollmentSampleStatus::kGood));
      response_map.emplace(
          static_cast<int>(BioEnrollmentResponseKey::kRemainingSamples),
          --mutable_state()->bio_remaining_samples);
      break;
    case SubCmd::kEnrollCaptureNextSample:
      if (!mutable_state()->bio_current_template_id ||
          mutable_state()->bio_current_template_id != *template_id) {
        NOTREACHED() << "Invalid current enrollment or template id parameter.";
        return CtapDeviceResponseCode::kCtap2ErrInvalidCBOR;
      }
      if (mutable_state()->bio_enrollment_next_sample_error) {
        response_map.emplace(
            static_cast<int>(BioEnrollmentResponseKey::kLastEnrollSampleStatus),
            static_cast<int>(BioEnrollmentSampleStatus::kTooHigh));
        response_map.emplace(
            static_cast<int>(BioEnrollmentResponseKey::kRemainingSamples),
            mutable_state()->bio_remaining_samples);
        mutable_state()->bio_enrollment_next_sample_error = false;
        break;
      }
      if (mutable_state()->bio_enrollment_next_sample_timeout) {
        response_map.emplace(
            static_cast<int>(BioEnrollmentResponseKey::kLastEnrollSampleStatus),
            static_cast<int>(BioEnrollmentSampleStatus::kNoUserActivity));
        response_map.emplace(
            static_cast<int>(BioEnrollmentResponseKey::kRemainingSamples),
            mutable_state()->bio_remaining_samples);
        mutable_state()->bio_enrollment_next_sample_timeout = false;
        break;
      }
      response_map.emplace(
          static_cast<int>(BioEnrollmentResponseKey::kLastEnrollSampleStatus),
          static_cast<int>(BioEnrollmentSampleStatus::kGood));
      response_map.emplace(
          static_cast<int>(BioEnrollmentResponseKey::kRemainingSamples),
          --mutable_state()->bio_remaining_samples);

      if (mutable_state()->bio_remaining_samples == 0) {
        mutable_state()
            ->bio_templates[*mutable_state()->bio_current_template_id] =
            base::StrCat(
                {"Template", base::NumberToString(
                                 *mutable_state()->bio_current_template_id)});
        mutable_state()->bio_current_template_id = base::nullopt;
        mutable_state()->fingerprints_enrolled = true;
      }
      break;
    case SubCmd::kEnumerateEnrollments: {
      if (mutable_state()->bio_templates.empty()) {
        return CtapDeviceResponseCode::kCtap2ErrInvalidOption;
      }
      cbor::Value::ArrayValue template_infos;
      for (const auto& enroll : mutable_state()->bio_templates) {
        cbor::Value::MapValue template_info;
        template_info.emplace(cbor::Value(static_cast<int>(
                                  BioEnrollmentTemplateInfoParam::kTemplateId)),
                              std::vector<uint8_t>{enroll.first});
        template_info.emplace(
            cbor::Value(static_cast<int>(
                BioEnrollmentTemplateInfoParam::kTemplateFriendlyName)),
            cbor::Value(enroll.second));
        template_infos.emplace_back(std::move(template_info));
      }
      response_map.emplace(
          static_cast<int>(BioEnrollmentResponseKey::kTemplateInfos),
          std::move(template_infos));
      break;
    }
    case SubCmd::kSetFriendlyName:
      if (!template_id || !name) {
        NOTREACHED() << "Could not parse template_id or name from parameters.";
        return CtapDeviceResponseCode::kCtap2ErrInvalidCBOR;
      }

      // Template ID from parameter does not exist, cannot rename.
      if (mutable_state()->bio_templates.find(*template_id) ==
          mutable_state()->bio_templates.end()) {
        return CtapDeviceResponseCode::kCtap2ErrInvalidOption;
      }

      mutable_state()->bio_templates[*template_id] = *name;
      return CtapDeviceResponseCode::kSuccess;
    case SubCmd::kRemoveEnrollment:
      if (!template_id) {
        NOTREACHED() << "Could not parse template_id or name from parameters.";
        return CtapDeviceResponseCode::kCtap2ErrInvalidCBOR;
      }

      // Template ID from parameter does not exist, cannot remove.
      if (mutable_state()->bio_templates.find(*template_id) ==
          mutable_state()->bio_templates.end()) {
        return CtapDeviceResponseCode::kCtap2ErrInvalidOption;
      }

      mutable_state()->bio_templates.erase(*template_id);
      return CtapDeviceResponseCode::kSuccess;
    case SubCmd::kCancelCurrentEnrollment:
      mutable_state()->bio_current_template_id = base::nullopt;
      return CtapDeviceResponseCode::kSuccess;
    default:
      // Handle all other commands as if they were unsupported (will change
      // when support is added).
      return CtapDeviceResponseCode::kCtap2ErrUnsupportedOption;
  }
  *response = cbor::Writer::Write(cbor::Value(std::move(response_map))).value();
  return CtapDeviceResponseCode::kSuccess;
}

void VirtualCtap2Device::InitPendingRPs() {
  mutable_state()->pending_rps.clear();
  std::set<std::string> rp_ids;
  for (const auto& registration : mutable_state()->registrations) {
    if (!registration.second.is_resident) {
      continue;
    }
    DCHECK(!registration.second.is_u2f);
    DCHECK(registration.second.user);
    DCHECK(registration.second.rp);
    if (!base::Contains(rp_ids, registration.second.rp->id)) {
      mutable_state()->pending_rps.push_back(*registration.second.rp);
    }
  }
}

void VirtualCtap2Device::InitPendingRegistrations(
    base::span<const uint8_t> rp_id_hash) {
  DCHECK_EQ(rp_id_hash.size(), kRpIdHashLength);
  mutable_state()->pending_registrations.clear();
  for (const auto& registration : mutable_state()->registrations) {
    if (!registration.second.is_resident ||
        !std::equal(rp_id_hash.begin(), rp_id_hash.end(),
                    registration.second.application_parameter.begin())) {
      continue;
    }
    DCHECK(!registration.second.is_u2f && registration.second.user &&
           registration.second.rp);
    cbor::Value::MapValue response_map;
    response_map.emplace(
        static_cast<int>(CredentialManagementResponseKey::kUser),
        *UserEntityAsCBOR(*registration.second.user,
                          config_.allow_invalid_utf8_in_credential_entities));
    response_map.emplace(
        static_cast<int>(CredentialManagementResponseKey::kCredentialID),
        AsCBOR(PublicKeyCredentialDescriptor(CredentialType::kPublicKey,
                                             registration.first)));

    base::Optional<cbor::Value> cose_key = cbor::Reader::Read(
        registration.second.private_key->GetPublicKey()->cose_key_bytes());
    response_map.emplace(
        static_cast<int>(CredentialManagementResponseKey::kPublicKey),
        cose_key->GetMap());
    mutable_state()->pending_registrations.emplace_back(
        std::move(response_map));
  }
}

void VirtualCtap2Device::GetNextRP(cbor::Value::MapValue* response_map) {
  DCHECK(!mutable_state()->pending_rps.empty());
  response_map->emplace(
      static_cast<int>(CredentialManagementResponseKey::kRP),
      *RpEntityAsCBOR(mutable_state()->pending_rps.front(),
                      config_.allow_invalid_utf8_in_credential_entities));
  response_map->emplace(
      static_cast<int>(CredentialManagementResponseKey::kRPIDHash),
      fido_parsing_utils::CreateSHA256Hash(
          mutable_state()->pending_rps.front().id));
  mutable_state()->pending_rps.pop_front();
}

CtapDeviceResponseCode VirtualCtap2Device::OnAuthenticatorGetInfo(
    std::vector<uint8_t>* response) const {
  *response = AuthenticatorGetInfoResponse::EncodeToCBOR(*device_info_);
  return CtapDeviceResponseCode::kSuccess;
}

AttestedCredentialData VirtualCtap2Device::ConstructAttestedCredentialData(
    base::span<const uint8_t> key_handle,
    std::unique_ptr<PublicKey> public_key) {
  constexpr std::array<uint8_t, 2> sha256_length = {0, crypto::kSHA256Length};
  constexpr std::array<uint8_t, 16> kZeroAaguid = {0, 0, 0, 0, 0, 0, 0, 0,
                                                   0, 0, 0, 0, 0, 0, 0, 0};
  base::span<const uint8_t, 16> aaguid(kDeviceAaguid);
  if (mutable_state()->self_attestation &&
      !mutable_state()->non_zero_aaguid_with_self_attestation) {
    aaguid = kZeroAaguid;
  }
  return AttestedCredentialData(aaguid, sha256_length,
                                fido_parsing_utils::Materialize(key_handle),
                                std::move(public_key));
}

AuthenticatorData VirtualCtap2Device::ConstructAuthenticatorData(
    base::span<const uint8_t, kRpIdHashLength> rp_id_hash,
    bool user_verified,
    uint32_t current_signature_count,
    base::Optional<AttestedCredentialData> attested_credential_data,
    base::Optional<cbor::Value> extensions) {
  uint8_t flag =
      base::strict_cast<uint8_t>(AuthenticatorData::Flag::kTestOfUserPresence);
  if (user_verified) {
    flag |= base::strict_cast<uint8_t>(
        AuthenticatorData::Flag::kTestOfUserVerification);
  }
  if (attested_credential_data)
    flag |= base::strict_cast<uint8_t>(AuthenticatorData::Flag::kAttestation);
  if (extensions) {
    flag |= base::strict_cast<uint8_t>(
        AuthenticatorData::Flag::kExtensionDataIncluded);
  }

  std::array<uint8_t, kSignCounterLength> signature_counter;
  signature_counter[0] = (current_signature_count >> 24) & 0xff;
  signature_counter[1] = (current_signature_count >> 16) & 0xff;
  signature_counter[2] = (current_signature_count >> 8) & 0xff;
  signature_counter[3] = (current_signature_count)&0xff;

  return AuthenticatorData(rp_id_hash, flag, signature_counter,
                           std::move(attested_credential_data),
                           std::move(extensions));
}
}  // namespace device
