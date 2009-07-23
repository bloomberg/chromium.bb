// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/alt_error_page_resource_fetcher.h"

#include "webkit/glue/resource_fetcher.h"

using WebKit::WebURLError;
using WebKit::WebURLResponse;

namespace webkit_glue {

// Number of seconds to wait for the alternate error page server.  If it takes
// too long, just use the local error page.
static const int kDownloadTimeoutSec = 3;

AltErrorPageResourceFetcher::AltErrorPageResourceFetcher(
    const GURL& url,
    WebFrame* frame,
    const GURL& unreachable_url,
    Callback* callback)
    : callback_(callback),
      unreachable_url_(unreachable_url) {
  fetcher_.reset(new ResourceFetcherWithTimeout(
      url, frame, kDownloadTimeoutSec,
      NewCallback(this, &AltErrorPageResourceFetcher::OnURLFetchComplete)));
}

AltErrorPageResourceFetcher::~AltErrorPageResourceFetcher() {
}

void AltErrorPageResourceFetcher::Cancel() {
  fetcher_->Cancel();
}

void AltErrorPageResourceFetcher::OnURLFetchComplete(
    const WebURLResponse& response,
    const std::string& data) {
  // A null response indicates a network error.
  if (!response.isNull() && response.httpStatusCode() == 200) {
    callback_->Run(unreachable_url_, data);
  } else {
    callback_->Run(unreachable_url_, std::string());
  }
}

}  // namespace webkit_glue
