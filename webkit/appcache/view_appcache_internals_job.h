// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_VIEW_APPCACHE_INTERNALS_JOB_H_
#define WEBKIT_APPCACHE_VIEW_APPCACHE_INTERNALS_JOB_H_

#include "base/basictypes.h"

namespace net {
class URLRequest;
class URLRequestJob;
}

namespace appcache {

class AppCacheService;

class ViewAppCacheInternalsJobFactory {
 public:
  static net::URLRequestJob* CreateJobForRequest(
      net::URLRequest* request, AppCacheService* service);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ViewAppCacheInternalsJobFactory);
};

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_VIEW_APPCACHE_INTERNALS_JOB_H_
