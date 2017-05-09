// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SecurityOriginStructTraits_h
#define SecurityOriginStructTraits_h

#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/text/WTFString.h"
#include "url/mojo/origin.mojom-blink.h"

namespace mojo {

template <>
struct StructTraits<url::mojom::blink::Origin::DataView,
                    RefPtr<::blink::SecurityOrigin>> {
  static WTF::String scheme(const RefPtr<::blink::SecurityOrigin>& origin) {
    return origin->Protocol();
  }
  static WTF::String host(const RefPtr<::blink::SecurityOrigin>& origin) {
    return origin->Host();
  }
  static uint16_t port(const RefPtr<::blink::SecurityOrigin>& origin) {
    return origin->EffectivePort();
  }
  static WTF::String suborigin(const RefPtr<::blink::SecurityOrigin>& origin) {
    WTF::String suborigin = origin->GetSuborigin()->GetName();
    return suborigin.IsNull() ? "" : suborigin;
  }
  static bool unique(const RefPtr<::blink::SecurityOrigin>& origin) {
    return origin->IsUnique();
  }
  static bool Read(url::mojom::blink::Origin::DataView data,
                   RefPtr<::blink::SecurityOrigin>* out) {
    if (data.unique()) {
      *out = ::blink::SecurityOrigin::CreateUnique();
    } else {
      WTF::String scheme;
      WTF::String host;
      WTF::String suborigin;
      if (!data.ReadScheme(&scheme) || !data.ReadHost(&host) ||
          !data.ReadSuborigin(&suborigin))
        return false;

      *out =
          ::blink::SecurityOrigin::Create(scheme, host, data.port(), suborigin);
    }

    // If a unique origin was created, but the unique flag wasn't set, then
    // the values provided to 'create' were invalid.
    if (!data.unique() && (*out)->IsUnique())
      return false;

    return true;
  }
};
}

#endif  // SecurityOriginStructTraits_h
