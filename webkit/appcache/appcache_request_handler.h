// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_APPCACHE_REQUEST_HANDLER_H_
#define WEBKIT_APPCACHE_APPCACHE_REQUEST_HANDLER_H_

#include "net/url_request/url_request.h"
#include "webkit/appcache/appcache_host.h"

class URLRequest;
class URLRequestJob;

namespace appcache {

// An instance is created for each URLRequest. The instance survives all
// http transactions involved in the processing of its URLRequest, and is
// given the opportunity to hijack the request along the way. Callers
// should use AppCacheHost::CreateRequestHandler to manufacture instances
// that can retrieve resources for a particular host.
class AppCacheRequestHandler : public URLRequest::UserData,
                               public AppCacheHost::Observer {
 public:
  virtual ~AppCacheRequestHandler();

  // Should be called on each request intercept opportunity.
  URLRequestJob* MaybeLoadResource(URLRequest* request);
  URLRequestJob* MaybeLoadFallbackForRedirect(URLRequest* request,
                                              const GURL& location);
  URLRequestJob* MaybeLoadFallbackForResponse(URLRequest* request);

  void GetExtraResponseInfo(int64* cache_id, GURL* manifest_url);

 private:
  friend class AppCacheHost;

  // Callers should use AppCacheHost::CreateRequestHandler.
  AppCacheRequestHandler(AppCacheHost* host, bool is_main_resource);

  // AppCacheHost::Observer methods
  virtual void OnCacheSelectionComplete(AppCacheHost* host);
  virtual void OnDestructionImminent(AppCacheHost* host);

  // Main vs subresource loads are very different.
  // TODO(michaeln): maybe have two derived classes?
  bool is_main_request_;
  AppCacheHost* host_;
  scoped_refptr<AppCache> cache_;
  scoped_refptr<URLRequestJob> job_;
};

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_APPCACHE_REQUEST_HANDLER_H_

