// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/renderer/appcache/appcache_frontend_impl.h"

#include "base/logging.h"
#include "third_party/WebKit/public/web/WebApplicationCacheHost.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "webkit/renderer/appcache/web_application_cache_host_impl.h"

using WebKit::WebApplicationCacheHost;
using WebKit::WebConsoleMessage;

namespace appcache {

// Inline helper to keep the lines shorter and unwrapped.
inline WebApplicationCacheHostImpl* GetHost(int id) {
  return WebApplicationCacheHostImpl::FromId(id);
}

void AppCacheFrontendImpl::OnCacheSelected(
    int host_id, const AppCacheInfo& info) {
  WebApplicationCacheHostImpl* host = GetHost(host_id);
  if (host)
    host->OnCacheSelected(info);
}

void AppCacheFrontendImpl::OnStatusChanged(const std::vector<int>& host_ids,
                                           Status status) {
  for (std::vector<int>::const_iterator i = host_ids.begin();
       i != host_ids.end(); ++i) {
    WebApplicationCacheHostImpl* host = GetHost(*i);
    if (host)
      host->OnStatusChanged(status);
  }
}

void AppCacheFrontendImpl::OnEventRaised(const std::vector<int>& host_ids,
                                         EventID event_id) {
  DCHECK(event_id != PROGRESS_EVENT);  // See OnProgressEventRaised.
  DCHECK(event_id != ERROR_EVENT);  // See OnErrorEventRaised.
  for (std::vector<int>::const_iterator i = host_ids.begin();
       i != host_ids.end(); ++i) {
    WebApplicationCacheHostImpl* host = GetHost(*i);
    if (host)
      host->OnEventRaised(event_id);
  }
}

void AppCacheFrontendImpl::OnProgressEventRaised(
    const std::vector<int>& host_ids,
    const GURL& url, int num_total, int num_complete) {
  for (std::vector<int>::const_iterator i = host_ids.begin();
       i != host_ids.end(); ++i) {
    WebApplicationCacheHostImpl* host = GetHost(*i);
    if (host)
      host->OnProgressEventRaised(url, num_total, num_complete);
  }
}

void AppCacheFrontendImpl::OnErrorEventRaised(
    const std::vector<int>& host_ids,
    const std::string& message) {
  for (std::vector<int>::const_iterator i = host_ids.begin();
       i != host_ids.end(); ++i) {
    WebApplicationCacheHostImpl* host = GetHost(*i);
    if (host)
      host->OnErrorEventRaised(message);
  }
}

void AppCacheFrontendImpl::OnLogMessage(int host_id, LogLevel log_level,
                                        const std::string& message) {
  WebApplicationCacheHostImpl* host = GetHost(host_id);
  if (host)
    host->OnLogMessage(log_level, message);
}

void AppCacheFrontendImpl::OnContentBlocked(int host_id,
                                            const GURL& manifest_url) {
  WebApplicationCacheHostImpl* host = GetHost(host_id);
  if (host)
    host->OnContentBlocked(manifest_url);
}

// Ensure that enum values never get out of sync with the
// ones declared for use within the WebKit api
COMPILE_ASSERT((int)WebApplicationCacheHost::Uncached ==
               (int)UNCACHED, Uncached);
COMPILE_ASSERT((int)WebApplicationCacheHost::Idle ==
               (int)IDLE, Idle);
COMPILE_ASSERT((int)WebApplicationCacheHost::Checking ==
               (int)CHECKING, Checking);
COMPILE_ASSERT((int)WebApplicationCacheHost::Downloading ==
               (int)DOWNLOADING, Downloading);
COMPILE_ASSERT((int)WebApplicationCacheHost::UpdateReady ==
               (int)UPDATE_READY, UpdateReady);
COMPILE_ASSERT((int)WebApplicationCacheHost::Obsolete ==
               (int)OBSOLETE, Obsolete);
COMPILE_ASSERT((int)WebApplicationCacheHost::CheckingEvent ==
               (int)CHECKING_EVENT, CheckingEvent);
COMPILE_ASSERT((int)WebApplicationCacheHost::ErrorEvent ==
               (int)ERROR_EVENT, ErrorEvent);
COMPILE_ASSERT((int)WebApplicationCacheHost::NoUpdateEvent ==
               (int)NO_UPDATE_EVENT, NoUpdateEvent);
COMPILE_ASSERT((int)WebApplicationCacheHost::DownloadingEvent ==
               (int)DOWNLOADING_EVENT, DownloadingEvent);
COMPILE_ASSERT((int)WebApplicationCacheHost::ProgressEvent ==
               (int)PROGRESS_EVENT, ProgressEvent);
COMPILE_ASSERT((int)WebApplicationCacheHost::UpdateReadyEvent ==
               (int)UPDATE_READY_EVENT, UpdateReadyEvent);
COMPILE_ASSERT((int)WebApplicationCacheHost::CachedEvent ==
               (int)CACHED_EVENT, CachedEvent);
COMPILE_ASSERT((int)WebApplicationCacheHost::ObsoleteEvent ==
               (int)OBSOLETE_EVENT, ObsoleteEvent);
COMPILE_ASSERT((int)WebConsoleMessage::LevelDebug ==
               (int)LOG_DEBUG, LevelDebug);
COMPILE_ASSERT((int)WebConsoleMessage::LevelLog ==
               (int)LOG_INFO, LevelLog);
COMPILE_ASSERT((int)WebConsoleMessage::LevelWarning ==
               (int)LOG_WARNING, LevelWarning);
COMPILE_ASSERT((int)WebConsoleMessage::LevelError ==
               (int)LOG_ERROR, LevelError);

}  // namespace appcache
