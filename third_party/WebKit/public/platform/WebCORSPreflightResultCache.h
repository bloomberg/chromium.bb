/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef WebCORSPreflightResultCache_h
#define WebCORSPreflightResultCache_h

#include <memory>
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/WebURLResponse.h"

namespace blink {

class HTTPHeaderMap;

// Represents an entry of the CORS-preflight cache.
// See https://fetch.spec.whatwg.org/#concept-cache.
class BLINK_PLATFORM_EXPORT WebCORSPreflightResultCacheItem {
  WTF_MAKE_NONCOPYABLE(WebCORSPreflightResultCacheItem);
  USING_FAST_MALLOC(WebCORSPreflightResultCacheItem);

 public:
  static std::unique_ptr<WebCORSPreflightResultCacheItem> Create(
      const WebURLRequest::FetchCredentialsMode,
      const HTTPHeaderMap&,
      WebString& error_description);

  bool AllowsCrossOriginMethod(const WebString&,
                               WebString& error_description) const;
  bool AllowsCrossOriginHeaders(const HTTPHeaderMap&,
                                WebString& error_description) const;
  bool AllowsRequest(WebURLRequest::FetchCredentialsMode,
                     const WebString& method,
                     const HTTPHeaderMap& request_headers) const;

 private:
  typedef HashSet<String, CaseFoldingHash> HeadersSet;

  explicit WebCORSPreflightResultCacheItem(WebURLRequest::FetchCredentialsMode);

  bool Parse(const HTTPHeaderMap& response_header,
             WebString& error_description);

  // FIXME: A better solution to holding onto the absolute expiration time might
  // be to start a timer for the expiration delta that removes this from the
  // cache when it fires.
  double absolute_expiry_time_;

  // Corresponds to the fields of the CORS-preflight cache with the same name.
  bool credentials_;
  HashSet<String> methods_;
  HeadersSet headers_;
};

class BLINK_PLATFORM_EXPORT WebCORSPreflightResultCache {
  WTF_MAKE_NONCOPYABLE(WebCORSPreflightResultCache);
  USING_FAST_MALLOC(WebCORSPreflightResultCache);

 public:
  static WebCORSPreflightResultCache& Shared();

  void AppendEntry(const WebString& origin,
                   const WebURL&,
                   std::unique_ptr<WebCORSPreflightResultCacheItem>);
  bool CanSkipPreflight(const WebString& origin,
                        const WebURL&,
                        WebURLRequest::FetchCredentialsMode,
                        const WebString& method,
                        const HTTPHeaderMap& request_headers);

 protected:
  // Protected for tests:
  WebCORSPreflightResultCache() {}

  typedef std::map<
      std::string,
      std::map<std::string, std::unique_ptr<WebCORSPreflightResultCacheItem>>>
      WebCORSPreflightResultHashMap;

  WebCORSPreflightResultHashMap preflight_hash_map_;
};

}  // namespace blink

#endif
