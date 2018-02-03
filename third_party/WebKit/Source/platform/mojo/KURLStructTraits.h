// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef KURLStructTraits_h
#define KURLStructTraits_h

#include "platform/weborigin/KURL.h"
#include "platform/wtf/text/WTFString.h"
#include "url/mojo/url.mojom-blink.h"
#include "url/url_constants.h"

namespace mojo {

template <>
struct StructTraits<url::mojom::blink::Url::DataView, ::blink::KURL> {
  static WTF::String url(const ::blink::KURL& blinkUrl) {
    if (!blinkUrl.IsValid() ||
        blinkUrl.GetString().length() > url::kMaxURLChars) {
      return g_empty_string;
    }

    return blinkUrl.GetString();
  }
  static bool Read(url::mojom::blink::Url::DataView data, ::blink::KURL* out) {
    WTF::String urlString;
    if (!data.ReadUrl(&urlString))
      return false;

    if (urlString.length() > url::kMaxURLChars)
      return false;

    *out = ::blink::KURL(::blink::KURL(), urlString);
    if (!urlString.IsEmpty() && !out->IsValid())
      return false;

    return true;
  }
};
}

#endif  // KURLStructTraits_h
