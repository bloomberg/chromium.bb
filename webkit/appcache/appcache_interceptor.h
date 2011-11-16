// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_APPCACHE_INTERCEPTOR_H_
#define WEBKIT_APPCACHE_APPCACHE_INTERCEPTOR_H_

#include "base/memory/singleton.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request.h"
#include "webkit/appcache/appcache_export.h"
#include "webkit/glue/resource_type.h"

namespace appcache {

class AppCacheRequestHandler;
class AppCacheService;

// An interceptor to hijack requests and potentially service them out of
// the appcache.
class APPCACHE_EXPORT AppCacheInterceptor
    : public net::URLRequest::Interceptor {
 public:
  // Registers a singleton instance with the net library.
  // Should be called early in the IO thread prior to initiating requests.
  static void EnsureRegistered() {
    CHECK(GetInstance());
  }

  // Must be called to make a request eligible for retrieval from an appcache.
  static void SetExtraRequestInfo(net::URLRequest* request,
                                  AppCacheService* service,
                                  int process_id,
                                  int host_id,
                                  ResourceType::Type resource_type);

  // May be called after response headers are complete to retrieve extra
  // info about the response.
  static void GetExtraResponseInfo(net::URLRequest* request,
                                   int64* cache_id,
                                   GURL* manifest_url);

  static AppCacheInterceptor* GetInstance();

 protected:
  // Override from net::URLRequest::Interceptor:
  virtual net::URLRequestJob* MaybeIntercept(net::URLRequest* request) OVERRIDE;
  virtual net::URLRequestJob* MaybeInterceptResponse(
      net::URLRequest* request) OVERRIDE;
  virtual net::URLRequestJob* MaybeInterceptRedirect(
      net::URLRequest* request,
      const GURL& location) OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<AppCacheInterceptor>;

  AppCacheInterceptor();
  virtual ~AppCacheInterceptor();

  static void SetHandler(net::URLRequest* request,
                         AppCacheRequestHandler* handler);
  static AppCacheRequestHandler* GetHandler(net::URLRequest* request);

  DISALLOW_COPY_AND_ASSIGN(AppCacheInterceptor);
};

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_APPCACHE_INTERCEPTOR_H_
