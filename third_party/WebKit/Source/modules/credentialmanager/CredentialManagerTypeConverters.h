// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CredentialManagerTypeConverters_h
#define CredentialManagerTypeConverters_h

#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/modules/webauth/authenticator.mojom-blink.h"

namespace blink {
class ArrayBufferOrArrayBufferView;
class MakePublicKeyCredentialOptions;
class PublicKeyCredentialParameters;
class PublicKeyCredentialRpEntity;
class PublicKeyCredentialUserEntity;
}  // namespace blink

namespace mojo {

// webauth::mojom::blink::Authenticator ---------------------------------------

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

}  // namespace mojo

#endif  // CredentialManagerProxy_h
