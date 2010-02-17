// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_APPCACHE_POLICY_H_
#define WEBKIT_APPCACHE_APPCACHE_POLICY_H_

#include "net/base/completion_callback.h"

class GURL;

namespace appcache {

class AppCachePolicy {
 public:
  AppCachePolicy() {}

  // Called prior to loading a main resource from the appache.
  // Returns true if allowed. This is expected to return immediately
  // without any user prompt.
  virtual bool CanLoadAppCache(const GURL& manifest_url) = 0;

  // Called prior to creating a new appcache.
  // Returns net::OK if allowed, net::ERR_ACCESS_DENIED if not allowed.
  // May also return net::ERR_IO_PENDING to indicate
  // that the completion callback will be notified (asynchronously and on
  // the current thread) of the final result.  Note: The completion callback
  // must remain valid until notified.
  virtual int CanCreateAppCache(const GURL& manifest_url,
                                net::CompletionCallback* callback) = 0;

 protected:
  ~AppCachePolicy() {}
};

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_APPCACHE_POLICY_H_
