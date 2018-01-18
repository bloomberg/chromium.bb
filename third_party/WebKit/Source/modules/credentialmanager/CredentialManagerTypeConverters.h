// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CredentialManagerTypeConverters_h
#define CredentialManagerTypeConverters_h

#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/modules/credentialmanager/credential_manager.mojom-blink.h"
#include "public/platform/modules/webauth/authenticator.mojom-blink.h"

namespace blink {
class ArrayBufferOrArrayBufferView;
class Credential;
class MakePublicKeyCredentialOptions;
class PublicKeyCredentialDescriptor;
class PublicKeyCredentialParameters;
class PublicKeyCredentialRequestOptions;
class PublicKeyCredentialRpEntity;
class PublicKeyCredentialUserEntity;
}  // namespace blink

namespace mojo {

// password_manager::mojom::blink::CredentialManager --------------------------

template <>
struct TypeConverter<password_manager::mojom::blink::CredentialInfoPtr,
                     blink::Credential*> {
  static password_manager::mojom::blink::CredentialInfoPtr Convert(
      blink::Credential*);
};

template <>
struct TypeConverter<blink::Credential*,
                     password_manager::mojom::blink::CredentialInfoPtr> {
  static blink::Credential* Convert(
      const password_manager::mojom::blink::CredentialInfoPtr&);
};

// webauth::mojom::blink::Authenticator ---------------------------------------
template <>
struct TypeConverter<password_manager::mojom::blink::CredentialManagerError,
                     webauth::mojom::blink::AuthenticatorStatus> {
  static password_manager::mojom::blink::CredentialManagerError Convert(
      const webauth::mojom::blink::AuthenticatorStatus&);
};

template <>
struct TypeConverter<Vector<uint8_t>, blink::ArrayBufferOrArrayBufferView> {
  static Vector<uint8_t> Convert(const blink::ArrayBufferOrArrayBufferView&);
};

template <>
struct TypeConverter<webauth::mojom::blink::PublicKeyCredentialType, String> {
  static webauth::mojom::blink::PublicKeyCredentialType Convert(
      const String& type);
};

template <>
struct TypeConverter<webauth::mojom::blink::AuthenticatorTransport, String> {
  static webauth::mojom::blink::AuthenticatorTransport Convert(
      const String& transport);
};

template <>
struct TypeConverter<webauth::mojom::blink::PublicKeyCredentialUserEntityPtr,
                     blink::PublicKeyCredentialUserEntity> {
  static webauth::mojom::blink::PublicKeyCredentialUserEntityPtr Convert(
      const blink::PublicKeyCredentialUserEntity&);
};

template <>
struct TypeConverter<webauth::mojom::blink::PublicKeyCredentialRpEntityPtr,
                     blink::PublicKeyCredentialRpEntity> {
  static webauth::mojom::blink::PublicKeyCredentialRpEntityPtr Convert(
      const blink::PublicKeyCredentialRpEntity&);
};

template <>
struct TypeConverter<webauth::mojom::blink::PublicKeyCredentialDescriptorPtr,
                     blink::PublicKeyCredentialDescriptor> {
  static webauth::mojom::blink::PublicKeyCredentialDescriptorPtr Convert(
      const blink::PublicKeyCredentialDescriptor&);
};

template <>
struct TypeConverter<webauth::mojom::blink::PublicKeyCredentialParametersPtr,
                     blink::PublicKeyCredentialParameters> {
  static webauth::mojom::blink::PublicKeyCredentialParametersPtr Convert(
      const blink::PublicKeyCredentialParameters&);
};

template <>
struct TypeConverter<webauth::mojom::blink::MakePublicKeyCredentialOptionsPtr,
                     blink::MakePublicKeyCredentialOptions> {
  static webauth::mojom::blink::MakePublicKeyCredentialOptionsPtr Convert(
      const blink::MakePublicKeyCredentialOptions&);
};

template <>
struct TypeConverter<
    webauth::mojom::blink::PublicKeyCredentialRequestOptionsPtr,
    blink::PublicKeyCredentialRequestOptions> {
  static webauth::mojom::blink::PublicKeyCredentialRequestOptionsPtr Convert(
      const blink::PublicKeyCredentialRequestOptions&);
};

}  // namespace mojo

#endif  // CredentialManagerProxy_h
