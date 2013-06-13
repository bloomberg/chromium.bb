// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_APPCACHE_VIEW_APPCACHE_INTERNALS_JOB_H_
#define WEBKIT_BROWSER_APPCACHE_VIEW_APPCACHE_INTERNALS_JOB_H_

#include "base/basictypes.h"
#include "webkit/browser/webkit_storage_browser_export.h"

namespace net {
class NetworkDelegate;
class URLRequest;
class URLRequestJob;
}

namespace appcache {

class AppCacheService;

class WEBKIT_STORAGE_BROWSER_EXPORT ViewAppCacheInternalsJobFactory {
 public:
  static net::URLRequestJob* CreateJobForRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      AppCacheService* service);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ViewAppCacheInternalsJobFactory);
};

}  // namespace appcache

#endif  // WEBKIT_BROWSER_APPCACHE_VIEW_APPCACHE_INTERNALS_JOB_H_
