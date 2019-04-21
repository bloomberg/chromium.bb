// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/url_request_mock_util.h"

#include <string>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/test/url_request/url_request_mock_http_job.h"
#include "net/test/url_request/url_request_slow_download_job.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_test_job.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace chrome_browser_net {

void SetUrlRequestMocksEnabled(bool enabled) {
  // Since this involves changing the net::URLRequest ProtocolFactory, we need
  // to run on the IO thread.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (enabled) {
    // We have to look around for our helper files, but we only use
    // this from tests, so allow these IO operations to happen
    // anywhere.
    base::ThreadRestrictions::ScopedAllowIO allow_io;

    net::URLRequestFilter::GetInstance()->ClearHandlers();

    net::URLRequestFailedJob::AddUrlHandler();
    net::URLRequestSlowDownloadJob::AddUrlHandler();

    base::FilePath root_http;
    base::PathService::Get(chrome::DIR_TEST_DATA, &root_http);
    net::URLRequestMockHTTPJob::AddUrlHandlers(root_http);
  } else {
    // Revert to the default handlers.
    net::URLRequestFilter::GetInstance()->ClearHandlers();
  }
}

}  // namespace chrome_browser_net
