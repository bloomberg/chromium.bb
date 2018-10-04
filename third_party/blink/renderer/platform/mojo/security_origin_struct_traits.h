// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_SECURITY_ORIGIN_STRUCT_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_SECURITY_ORIGIN_STRUCT_TRAITS_H_

#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "url/mojom/origin.mojom-blink.h"

namespace mojo {

template <>
struct StructTraits<url::mojom::blink::Origin::DataView,
                    scoped_refptr<const ::blink::SecurityOrigin>> {
  static WTF::String scheme(
      const scoped_refptr<const ::blink::SecurityOrigin>& origin) {
    return origin->Protocol();
  }
  static WTF::String host(
      const scoped_refptr<const ::blink::SecurityOrigin>& origin) {
    return origin->Host();
  }
  static uint16_t port(
      const scoped_refptr<const ::blink::SecurityOrigin>& origin) {
    return origin->EffectivePort();
  }
  static base::Optional<base::UnguessableToken> nonce_if_opaque(
      const scoped_refptr<const ::blink::SecurityOrigin>& origin) {
    if (origin->IsOpaque())
      return base::UnguessableToken::Create();

    return base::nullopt;
  }
  static bool Read(url::mojom::blink::Origin::DataView data,
                   scoped_refptr<const ::blink::SecurityOrigin>* out) {
    WTF::String scheme;
    WTF::String host;
    base::Optional<base::UnguessableToken> nonce_if_opaque;
    if (!data.ReadScheme(&scheme) || !data.ReadHost(&host) ||
        !data.ReadNonceIfOpaque(&nonce_if_opaque))
      return false;

    if (nonce_if_opaque) {
      *out = blink::SecurityOrigin::CreateUniqueOpaque();
      return true;
    }

    *out = blink::SecurityOrigin::Create(scheme, host, data.port());

    // If an opaque origin was created, but the opaque flag wasn't set, then
    // the values provided to 'Create' were invalid.
    if ((*out)->IsOpaque())
      return false;

    return true;
  }
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_SECURITY_ORIGIN_STRUCT_TRAITS_H_
