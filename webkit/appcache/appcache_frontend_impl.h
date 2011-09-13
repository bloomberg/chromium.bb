// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_APPCACHE_FRONTEND_IMPL_H_
#define WEBKIT_APPCACHE_APPCACHE_FRONTEND_IMPL_H_

#include <string>
#include <vector>
#include "webkit/appcache/appcache_interfaces.h"

namespace appcache {

class AppCacheFrontendImpl : public AppCacheFrontend {
 public:
  virtual void OnCacheSelected(
      int host_id, const appcache::AppCacheInfo& info);
  virtual void OnStatusChanged(const std::vector<int>& host_ids,
                               Status status);
  virtual void OnEventRaised(const std::vector<int>& host_ids,
                             EventID event_id);
  virtual void OnProgressEventRaised(const std::vector<int>& host_ids,
                                     const GURL& url,
                                     int num_total, int num_complete);
  virtual void OnErrorEventRaised(const std::vector<int>& host_ids,
                                  const std::string& message);
  virtual void OnLogMessage(int host_id, LogLevel log_level,
                            const std::string& message);
  virtual void OnContentBlocked(int host_id, const GURL& manifest_url);
};

}  // namespace

#endif  // WEBKIT_APPCACHE_APPCACHE_FRONTEND_IMPL_H_
