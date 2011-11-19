// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/alt_error_page_resource_fetcher.h"

#include "base/callback.h"
#include "webkit/glue/resource_fetcher.h"

using WebKit::WebFrame;
using WebKit::WebURLError;
using WebKit::WebURLRequest;
using WebKit::WebURLResponse;

namespace webkit_glue {

// Number of seconds to wait for the alternate error page server.  If it takes
// too long, just use the local error page.
static const int kDownloadTimeoutSec = 3;

AltErrorPageResourceFetcher::AltErrorPageResourceFetcher(
    const GURL& url,
    WebFrame* frame,
    const WebURLError& original_error,
    const Callback& callback)
    : frame_(frame),
      callback_(callback),
      original_error_(original_error) {
  fetcher_.reset(new ResourceFetcherWithTimeout(
      url, frame, WebURLRequest::TargetIsMainFrame, kDownloadTimeoutSec,
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
    callback_.Run(frame_, original_error_, data);
  } else {
    callback_.Run(frame_, original_error_, std::string());
  }
}

}  // namespace webkit_glue
