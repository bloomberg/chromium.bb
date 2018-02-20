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
#include <string>
#include "base/containers/flat_set.h"
#include "base/time/time.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "public/platform/WebHTTPHeaderMap.h"
#include "public/platform/WebHTTPHeaderSet.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/WebURLResponse.h"
#include "services/network/public/cpp/cors/preflight_result.h"

namespace blink {

class BLINK_PLATFORM_EXPORT WebCORSPreflightResultCache {
 public:
  WebCORSPreflightResultCache(const WebCORSPreflightResultCache&) = delete;
  WebCORSPreflightResultCache& operator=(const WebCORSPreflightResultCache&) =
      delete;

  // Returns a WebCORSPreflightResultCache which is shared in the same thread.
  static WebCORSPreflightResultCache& Shared();

  // TODO(toyoshim): Move to platform/loader/cors, as
  // CORS::EnsurePreflightResultAndCacheOnSuccess when
  // WebCORSPreflightResultCache is ported to network service.
  bool EnsureResultAndMayAppendEntry(
      const WebHTTPHeaderMap& response_header_map,
      const WebString& origin,
      const WebURL& request_url,
      const WebString& request_method,
      const WebHTTPHeaderMap& request_header_map,
      network::mojom::FetchCredentialsMode request_credentials_mode,
      WebString* error_description);

  // TODO(toyoshim): Remove the following method that is used only for testing
  // outside this class implementation.
  void AppendEntry(const WebString& origin,
                   const WebURL&,
                   std::unique_ptr<network::cors::PreflightResult>);

  // TODO(toyoshim): Move to platform/loader/cors, as CORS::CanSkipPreflight
  // when WebCORSPreflightResultCache is ported to network service.
  bool CanSkipPreflight(const WebString& origin,
                        const WebURL&,
                        network::mojom::FetchCredentialsMode,
                        const WebString& method,
                        const WebHTTPHeaderMap& request_headers);

 protected:
  friend class WTF::ThreadSpecific<WebCORSPreflightResultCache>;

  WebCORSPreflightResultCache();
  virtual ~WebCORSPreflightResultCache();

  typedef std::map<
      std::string,
      std::map<std::string, std::unique_ptr<network::cors::PreflightResult>>>
      PreflightResultHashMap;

  PreflightResultHashMap preflight_hash_map_;
};

}  // namespace blink

#endif
