// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/authenticator_type_converters.h"

#include <algorithm>
#include <utility>

#include "base/containers/flat_set.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"

namespace mojo {

using ::blink::mojom::AttestationConveyancePreference;
using ::blink::mojom::AuthenticatorAttachment;
using ::blink::mojom::AuthenticatorSelectionCriteriaPtr;
using ::blink::mojom::AuthenticatorTransport;
using ::blink::mojom::CableAuthenticationPtr;
using ::blink::mojom::PublicKeyCredentialDescriptorPtr;
using ::blink::mojom::PublicKeyCredentialParametersPtr;
using ::blink::mojom::PublicKeyCredentialRpEntityPtr;
using ::blink::mojom::PublicKeyCredentialType;
using ::blink::mojom::PublicKeyCredentialUserEntityPtr;
using ::blink::mojom::UserVerificationRequirement;

// static
::device::FidoTransportProtocol
TypeConverter<::device::FidoTransportProtocol, AuthenticatorTransport>::Convert(
    const AuthenticatorTransport& input) {
  switch (input) {
    case AuthenticatorTransport::USB:
      return ::device::FidoTransportProtocol::kUsbHumanInterfaceDevice;
    case AuthenticatorTransport::NFC:
      return ::device::FidoTransportProtocol::kNearFieldCommunication;
    case AuthenticatorTransport::BLE:
      return ::device::FidoTransportProtocol::kBluetoothLowEnergy;
    case AuthenticatorTransport::CABLE:
      return ::device::FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy;
    case AuthenticatorTransport::INTERNAL:
      return ::device::FidoTransportProtocol::kInternal;
  }
  NOTREACHED();
  return ::device::FidoTransportProtocol::kUsbHumanInterfaceDevice;
}

AuthenticatorTransport
TypeConverter<AuthenticatorTransport, ::device::FidoTransportProtocol>::Convert(
    const ::device::FidoTransportProtocol& input) {
  switch (input) {
    case ::device::FidoTransportProtocol::kUsbHumanInterfaceDevice:
      return AuthenticatorTransport::USB;
    case ::device::FidoTransportProtocol::kNearFieldCommunication:
      return AuthenticatorTransport::NFC;
    case ::device::FidoTransportProtocol::kBluetoothLowEnergy:
      return AuthenticatorTransport::BLE;
    case ::device::FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy:
      return AuthenticatorTransport::CABLE;
    case ::device::FidoTransportProtocol::kInternal:
      return AuthenticatorTransport::INTERNAL;
  }
  NOTREACHED();
  return AuthenticatorTransport::USB;
}

// static
::device::CredentialType
TypeConverter<::device::CredentialType, PublicKeyCredentialType>::Convert(
    const PublicKeyCredentialType& input) {
  switch (input) {
    case PublicKeyCredentialType::PUBLIC_KEY:
      return ::device::CredentialType::kPublicKey;
  }
  NOTREACHED();
  return ::device::CredentialType::kPublicKey;
}

// static
std::vector<::device::PublicKeyCredentialParams::CredentialInfo>
TypeConverter<std::vector<::device::PublicKeyCredentialParams::CredentialInfo>,
              std::vector<PublicKeyCredentialParametersPtr>>::
    Convert(const std::vector<PublicKeyCredentialParametersPtr>& input) {
  std::vector<::device::PublicKeyCredentialParams::CredentialInfo>
      credential_params;
  credential_params.reserve(input.size());

  for (const auto& credential : input) {
    credential_params.emplace_back(
        ::device::PublicKeyCredentialParams::CredentialInfo{
            ConvertTo<::device::CredentialType>(credential->type),
            credential->algorithm_identifier});
  }

  return credential_params;
}

// static
std::vector<::device::PublicKeyCredentialDescriptor>
TypeConverter<std::vector<::device::PublicKeyCredentialDescriptor>,
              std::vector<PublicKeyCredentialDescriptorPtr>>::
    Convert(const std::vector<PublicKeyCredentialDescriptorPtr>& input) {
  std::vector<::device::PublicKeyCredentialDescriptor> credential_descriptors;
  credential_descriptors.reserve(input.size());

  for (const auto& credential : input) {
    base::flat_set<::device::FidoTransportProtocol> protocols;
    for (const auto& protocol : credential->transports) {
      protocols.emplace(ConvertTo<::device::FidoTransportProtocol>(protocol));
    }

    credential_descriptors.emplace_back(::device::PublicKeyCredentialDescriptor(
        ConvertTo<::device::CredentialType>(credential->type), credential->id,
        std::move(protocols)));
  }
  return credential_descriptors;
}

// static
::device::UserVerificationRequirement TypeConverter<
    ::device::UserVerificationRequirement,
    UserVerificationRequirement>::Convert(const UserVerificationRequirement&
                                              input) {
  switch (input) {
    case UserVerificationRequirement::PREFERRED:
      return ::device::UserVerificationRequirement::kPreferred;
    case UserVerificationRequirement::REQUIRED:
      return ::device::UserVerificationRequirement::kRequired;
    case UserVerificationRequirement::DISCOURAGED:
      return ::device::UserVerificationRequirement::kDiscouraged;
  }
  NOTREACHED();
  return ::device::UserVerificationRequirement::kPreferred;
}

// static
::device::AuthenticatorAttachment TypeConverter<
    ::device::AuthenticatorAttachment,
    AuthenticatorAttachment>::Convert(const AuthenticatorAttachment& input) {
  switch (input) {
    case AuthenticatorAttachment::NO_PREFERENCE:
      return ::device::AuthenticatorAttachment::kAny;
    case AuthenticatorAttachment::PLATFORM:
      return ::device::AuthenticatorAttachment::kPlatform;
    case AuthenticatorAttachment::CROSS_PLATFORM:
      return ::device::AuthenticatorAttachment::kCrossPlatform;
  }
  NOTREACHED();
  return ::device::AuthenticatorAttachment::kAny;
}

// static
::device::AuthenticatorSelectionCriteria
TypeConverter<::device::AuthenticatorSelectionCriteria,
              AuthenticatorSelectionCriteriaPtr>::
    Convert(const AuthenticatorSelectionCriteriaPtr& input) {
  return device::AuthenticatorSelectionCriteria(
      ConvertTo<::device::AuthenticatorAttachment>(
          input->authenticator_attachment),
      input->require_resident_key,
      ConvertTo<::device::UserVerificationRequirement>(
          input->user_verification));
}

// static
::device::PublicKeyCredentialRpEntity
TypeConverter<::device::PublicKeyCredentialRpEntity,
              PublicKeyCredentialRpEntityPtr>::
    Convert(const PublicKeyCredentialRpEntityPtr& input) {
  device::PublicKeyCredentialRpEntity rp_entity(input->id);
  rp_entity.SetRpName(input->name);
  if (input->icon)
    rp_entity.SetRpIconUrl(*input->icon);

  return rp_entity;
}

// static
::device::PublicKeyCredentialUserEntity
TypeConverter<::device::PublicKeyCredentialUserEntity,
              PublicKeyCredentialUserEntityPtr>::
    Convert(const PublicKeyCredentialUserEntityPtr& input) {
  device::PublicKeyCredentialUserEntity user_entity(input->id);
  user_entity.name = input->name;
  user_entity.display_name = input->display_name;
  if (input->icon)
    user_entity.icon_url = *input->icon;

  return user_entity;
}

// static
std::vector<::device::CableDiscoveryData>
TypeConverter<std::vector<::device::CableDiscoveryData>,
              std::vector<CableAuthenticationPtr>>::
    Convert(const std::vector<CableAuthenticationPtr>& input) {
  std::vector<::device::CableDiscoveryData> discovery_data;
  discovery_data.reserve(input.size());

  for (const auto& data : input) {
    ::device::EidArray client_eid;
    DCHECK_EQ(client_eid.size(), data->client_eid.size());
    std::copy(data->client_eid.begin(), data->client_eid.end(),
              client_eid.begin());

    ::device::EidArray authenticator_eid;
    DCHECK_EQ(authenticator_eid.size(), data->authenticator_eid.size());
    std::copy(data->authenticator_eid.begin(), data->authenticator_eid.end(),
              authenticator_eid.begin());

    ::device::SessionPreKeyArray session_pre_key;
    DCHECK_EQ(session_pre_key.size(), data->session_pre_key.size());
    std::copy(data->session_pre_key.begin(), data->session_pre_key.end(),
              session_pre_key.begin());

    discovery_data.push_back(::device::CableDiscoveryData{
        data->version, client_eid, authenticator_eid, session_pre_key});
  }

  return discovery_data;
}

// static
::device::AttestationConveyancePreference
TypeConverter<::device::AttestationConveyancePreference,
              AttestationConveyancePreference>::
    Convert(const AttestationConveyancePreference& input) {
  switch (input) {
    case AttestationConveyancePreference::NONE:
      return ::device::AttestationConveyancePreference::NONE;
    case AttestationConveyancePreference::INDIRECT:
      return ::device::AttestationConveyancePreference::INDIRECT;
    case AttestationConveyancePreference::DIRECT:
      return ::device::AttestationConveyancePreference::DIRECT;
    case AttestationConveyancePreference::ENTERPRISE:
      return ::device::AttestationConveyancePreference::ENTERPRISE;
  }
  NOTREACHED();
  return ::device::AttestationConveyancePreference::NONE;
}

}  // namespace mojo
