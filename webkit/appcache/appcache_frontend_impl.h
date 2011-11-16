// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_APPCACHE_FRONTEND_IMPL_H_
#define WEBKIT_APPCACHE_APPCACHE_FRONTEND_IMPL_H_

#include <string>
#include <vector>
#include "webkit/appcache/appcache_export.h"
#include "webkit/appcache/appcache_interfaces.h"

namespace appcache {

class APPCACHE_EXPORT AppCacheFrontendImpl : public AppCacheFrontend {
 public:
  virtual void OnCacheSelected(
      int host_id, const appcache::AppCacheInfo& info) OVERRIDE;
  virtual void OnStatusChanged(const std::vector<int>& host_ids,
                               Status status) OVERRIDE;
  virtual void OnEventRaised(const std::vector<int>& host_ids,
                             EventID event_id) OVERRIDE;
  virtual void OnProgressEventRaised(const std::vector<int>& host_ids,
                                     const GURL& url,
                                     int num_total, int num_complete) OVERRIDE;
  virtual void OnErrorEventRaised(const std::vector<int>& host_ids,
                                  const std::string& message) OVERRIDE;
  virtual void OnLogMessage(int host_id, LogLevel log_level,
                            const std::string& message) OVERRIDE;
  virtual void OnContentBlocked(int host_id, const GURL& manifest_url) OVERRIDE;
};

}  // namespace

#endif  // WEBKIT_APPCACHE_APPCACHE_FRONTEND_IMPL_H_
