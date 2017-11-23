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
class MakeCredentialOptions;
class PublicKeyCredentialEntity;
class PublicKeyCredentialParameters;
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
struct TypeConverter<webauth::mojom::blink::PublicKeyCredentialEntityPtr,
                     blink::PublicKeyCredentialUserEntity> {
  static webauth::mojom::blink::PublicKeyCredentialEntityPtr Convert(
      const blink::PublicKeyCredentialUserEntity&);
};

template <>
struct TypeConverter<webauth::mojom::blink::PublicKeyCredentialEntityPtr,
                     blink::PublicKeyCredentialEntity> {
  static webauth::mojom::blink::PublicKeyCredentialEntityPtr Convert(
      const blink::PublicKeyCredentialEntity& rp);
};

template <>
struct TypeConverter<webauth::mojom::blink::PublicKeyCredentialParametersPtr,
                     blink::PublicKeyCredentialParameters> {
  static webauth::mojom::blink::PublicKeyCredentialParametersPtr Convert(
      const blink::PublicKeyCredentialParameters&);
};

template <>
struct TypeConverter<webauth::mojom::blink::MakeCredentialOptionsPtr,
                     blink::MakeCredentialOptions> {
  static webauth::mojom::blink::MakeCredentialOptionsPtr Convert(
      const blink::MakeCredentialOptions&);
};

}  // namespace mojo

#endif  // CredentialManagerProxy_h
