// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/credentialmanager/CredentialManagerTypeConverters.h"

#include "bindings/core/v8/array_buffer_or_array_buffer_view.h"
#include "modules/credentialmanager/MakePublicKeyCredentialOptions.h"
#include "modules/credentialmanager/PublicKeyCredential.h"
#include "modules/credentialmanager/PublicKeyCredentialParameters.h"
#include "modules/credentialmanager/PublicKeyCredentialRpEntity.h"
#include "modules/credentialmanager/PublicKeyCredentialUserEntity.h"
#include "platform/wtf/Time.h"

namespace {
// Time to wait for an authenticator to successfully complete an operation.
constexpr TimeDelta kAdjustedTimeoutLower = TimeDelta::FromMinutes(1);
constexpr TimeDelta kAdjustedTimeoutUpper = TimeDelta::FromMinutes(2);
}  // namespace

namespace mojo {

using webauth::mojom::blink::AuthenticatorStatus;
using webauth::mojom::blink::MakePublicKeyCredentialOptionsPtr;
using webauth::mojom::blink::PublicKeyCredentialRpEntity;
using webauth::mojom::blink::PublicKeyCredentialRpEntityPtr;
using webauth::mojom::blink::PublicKeyCredentialUserEntity;
using webauth::mojom::blink::PublicKeyCredentialUserEntityPtr;
using webauth::mojom::blink::PublicKeyCredentialParameters;
using webauth::mojom::blink::PublicKeyCredentialParametersPtr;
using webauth::mojom::blink::PublicKeyCredentialType;
using webauth::mojom::blink::AuthenticatorTransport;

// static
Vector<uint8_t>
TypeConverter<Vector<uint8_t>, blink::ArrayBufferOrArrayBufferView>::Convert(
    const blink::ArrayBufferOrArrayBufferView& buffer) {
  DCHECK(!buffer.IsNull());
  Vector<uint8_t> vector;
  if (buffer.IsArrayBuffer()) {
    vector.Append(static_cast<uint8_t*>(buffer.GetAsArrayBuffer()->Data()),
                  buffer.GetAsArrayBuffer()->ByteLength());
  } else {
    DCHECK(buffer.IsArrayBufferView());
    vector.Append(static_cast<uint8_t*>(
                      buffer.GetAsArrayBufferView().View()->BaseAddress()),
                  buffer.GetAsArrayBufferView().View()->byteLength());
  }
  return vector;
}

// static
PublicKeyCredentialType TypeConverter<PublicKeyCredentialType, String>::Convert(
    const String& type) {
  if (type == "public-key")
    return PublicKeyCredentialType::PUBLIC_KEY;
  NOTREACHED();
  return PublicKeyCredentialType::PUBLIC_KEY;
}

// static
AuthenticatorTransport TypeConverter<AuthenticatorTransport, String>::Convert(
    const String& transport) {
  if (transport == "usb")
    return AuthenticatorTransport::USB;
  if (transport == "nfc")
    return AuthenticatorTransport::NFC;
  if (transport == "ble")
    return AuthenticatorTransport::BLE;
  NOTREACHED();
  return AuthenticatorTransport::USB;
}

// static
PublicKeyCredentialUserEntityPtr
TypeConverter<PublicKeyCredentialUserEntityPtr,
              blink::PublicKeyCredentialUserEntity>::
    Convert(const blink::PublicKeyCredentialUserEntity& user) {
  auto entity = webauth::mojom::blink::PublicKeyCredentialUserEntity::New();
  entity->id = ConvertTo<Vector<uint8_t>>(user.id());
  entity->name = user.name();
  if (user.hasIcon()) {
    entity->icon = blink::KURL(blink::KURL(), user.icon());
  }
  entity->display_name = user.displayName();
  return entity;
}

// static
PublicKeyCredentialRpEntityPtr
TypeConverter<PublicKeyCredentialRpEntityPtr,
              blink::PublicKeyCredentialRpEntity>::
    Convert(const blink::PublicKeyCredentialRpEntity& rp) {
  auto entity = webauth::mojom::blink::PublicKeyCredentialRpEntity::New();
  entity->id = rp.id();
  entity->name = rp.name();
  if (rp.hasIcon()) {
    entity->icon = blink::KURL(blink::KURL(), rp.icon());
  }
  return entity;
}

// static
PublicKeyCredentialParametersPtr
TypeConverter<PublicKeyCredentialParametersPtr,
              blink::PublicKeyCredentialParameters>::
    Convert(const blink::PublicKeyCredentialParameters& parameter) {
  auto mojo_parameter =
      webauth::mojom::blink::PublicKeyCredentialParameters::New();
  mojo_parameter->type = ConvertTo<PublicKeyCredentialType>(parameter.type());

  // A COSEAlgorithmIdentifier's value is a number identifying a cryptographic
  // algorithm. Values are registered in the IANA COSE Algorithms registry.
  // https://www.iana.org/assignments/cose/cose.xhtml#algorithms
  mojo_parameter->algorithm_identifier = parameter.alg();
  return mojo_parameter;
}

// static
MakePublicKeyCredentialOptionsPtr
TypeConverter<MakePublicKeyCredentialOptionsPtr,
              blink::MakePublicKeyCredentialOptions>::
    Convert(const blink::MakePublicKeyCredentialOptions& options) {
  auto mojo_options =
      webauth::mojom::blink::MakePublicKeyCredentialOptions::New();
  mojo_options->relying_party = PublicKeyCredentialRpEntity::From(options.rp());
  mojo_options->user = PublicKeyCredentialUserEntity::From(options.user());
  if (!mojo_options->relying_party | !mojo_options->user) {
    return nullptr;
  }
  mojo_options->challenge = ConvertTo<Vector<uint8_t>>(options.challenge());

  // Step 4 of https://w3c.github.io/webauthn/#createCredential
  if (options.hasTimeout()) {
    WTF::TimeDelta adjusted_timeout;
    adjusted_timeout = WTF::TimeDelta::FromMilliseconds(options.timeout());
    mojo_options->adjusted_timeout =
        std::max(kAdjustedTimeoutLower,
                 std::min(kAdjustedTimeoutUpper, adjusted_timeout));
  } else {
    mojo_options->adjusted_timeout = kAdjustedTimeoutLower;
  }

  // Steps 8 and 9 of
  // https://www.w3.org/TR/2017/WD-webauthn-20170505/#createCredential
  Vector<PublicKeyCredentialParametersPtr> parameters;
  for (const auto& parameter : options.pubKeyCredParams()) {
    PublicKeyCredentialParametersPtr normalized_parameter =
        PublicKeyCredentialParameters::From(parameter);
    if (normalized_parameter) {
      parameters.push_back(std::move(normalized_parameter));
    }
  }

  if (parameters.IsEmpty() && options.hasPubKeyCredParams()) {
    return nullptr;
  }

  mojo_options->public_key_parameters = std::move(parameters);

  if (options.hasExcludeCredentials()) {
    // Adds the excludeCredentials members
    for (const blink::PublicKeyCredentialDescriptor& descriptor :
         options.excludeCredentials()) {
      auto mojo_descriptor =
          webauth::mojom::blink::PublicKeyCredentialDescriptor::New();
      mojo_descriptor->type =
          ConvertTo<PublicKeyCredentialType>(descriptor.type());
      mojo_descriptor->id = ConvertTo<Vector<uint8_t>>((descriptor.id()));
      if (descriptor.hasTransports()) {
        for (const auto& transport : descriptor.transports()) {
          mojo_descriptor->transports.push_back(
              ConvertTo<AuthenticatorTransport>(transport));
        }
      }
      mojo_options->exclude_credentials.push_back(std::move(mojo_descriptor));
    }
  }
  return mojo_options;
}

}  // namespace mojo
