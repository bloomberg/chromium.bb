// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_APPCACHE_FRONTEND_IMPL_H_
#define WEBKIT_APPCACHE_APPCACHE_FRONTEND_IMPL_H_

#include <vector>
#include "webkit/appcache/appcache_interfaces.h"

namespace appcache {

class AppCacheFrontendImpl : public AppCacheFrontend {
 public:
  virtual void OnCacheSelected(int host_id, int64 cache_id ,
                               Status status);
  virtual void OnStatusChanged(const std::vector<int>& host_ids,
                               Status status);
  virtual void OnEventRaised(const std::vector<int>& host_ids,
                             EventID event_id);
};

}  // namespace

#endif  // WEBKIT_APPCACHE_APPCACHE_FRONTEND_IMPL_H_
