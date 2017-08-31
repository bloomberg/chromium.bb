// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebHTTPHeaderMap_h
#define WebHTTPHeaderMap_h

#include <memory>
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "public/platform/WebString.h"

#if INSIDE_BLINK
#include "platform/network/HTTPHeaderMap.h"
#endif

namespace blink {

// A HTTP header map that takes net:: types but internally stores them in
// blink::HTTPHeaderMap. Therefore, not every header fits into this map.
// Notably, multiple Set-Cookie header fields are needed to set multiple cookies
class BLINK_PLATFORM_EXPORT WebHTTPHeaderMap {
 public:
  ~WebHTTPHeaderMap();

  WebString Get(const WebString& name) const;

  explicit WebHTTPHeaderMap(const net::HttpRequestHeaders*);
  explicit WebHTTPHeaderMap(const net::HttpResponseHeaders*);

#if INSIDE_BLINK
  WebHTTPHeaderMap(const HTTPHeaderMap&);
  const HTTPHeaderMap& GetHTTPHeaderMap() const;
#endif

 private:
  class WebHTTPHeaderMapImpl;

  std::unique_ptr<WebHTTPHeaderMapImpl> implementation_;
};

}  // namespace blink

#endif  // WebHTTPHeaderMap_h
