// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPresentationSessionInfo_h
#define WebPresentationSessionInfo_h

#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"

namespace blink {

// A simple wrapper around blink::mojom::PresentationSessionInfo. Since the
// presentation API has not been Onion Soup-ed, there are portions implemented
// in Blink and the embedder. Because it is not possible to use the STL and WTF
// mojo bindings alongside each other, going between Blink and the embedder
// requires this indirection.
struct WebPresentationSessionInfo {
  WebPresentationSessionInfo(const WebURL& url, const WebString& id)
      : url(url), id(id) {}
  WebURL url;
  WebString id;
};

}  // namespace blink

#endif  // WebPresentationSessionInfo_h
