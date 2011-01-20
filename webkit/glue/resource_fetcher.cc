// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/resource_fetcher.h"

#include "base/logging.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKitClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLError.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLLoader.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURL.h"

using base::TimeDelta;
using WebKit::WebFrame;
using WebKit::WebURLError;
using WebKit::WebURLLoader;
using WebKit::WebURLRequest;
using WebKit::WebURLResponse;

namespace webkit_glue {

ResourceFetcher::ResourceFetcher(const GURL& url, WebFrame* frame,
                                 Callback* c)
    : url_(url),
      completed_(false),
      callback_(c) {
  // Can't do anything without a frame.  However, delegate can be NULL (so we
  // can do a http request and ignore the results).
  DCHECK(frame);
  Start(frame);
}

ResourceFetcher::~ResourceFetcher() {
  if (!completed_ && loader_.get())
    loader_->cancel();
}

void ResourceFetcher::Cancel() {
  if (!completed_) {
    loader_->cancel();
    completed_ = true;
  }
}

void ResourceFetcher::Start(WebFrame* frame) {
  WebURLRequest request(url_);
  frame->dispatchWillSendRequest(request);

  loader_.reset(WebKit::webKitClient()->createURLLoader());
  loader_->loadAsynchronously(request, this);
}

void ResourceFetcher::RunCallback(const WebURLResponse& response,
                                  const std::string& data) {
  if (!callback_.get())
    return;

  // Take care to clear callback_ before running the callback as it may lead to
  // our destruction.
  scoped_ptr<Callback> callback;
  callback.swap(callback_);
  callback->Run(response, data);
}

/////////////////////////////////////////////////////////////////////////////
// WebURLLoaderClient methods

void ResourceFetcher::willSendRequest(
    WebURLLoader* loader, WebURLRequest& new_request,
    const WebURLResponse& redirect_response) {
}

void ResourceFetcher::didSendData(
    WebURLLoader* loader, unsigned long long bytes_sent,
    unsigned long long total_bytes_to_be_sent) {
}

void ResourceFetcher::didReceiveResponse(
    WebURLLoader* loader, const WebURLResponse& response) {
  DCHECK(!completed_);
  response_ = response;
}

void ResourceFetcher::didReceiveData(
    WebURLLoader* loader, const char* data, int data_length) {
  DCHECK(!completed_);
  DCHECK(data_length > 0);

  data_.append(data, data_length);
}

void ResourceFetcher::didReceiveCachedMetadata(
    WebURLLoader* loader, const char* data, int data_length) {
  DCHECK(!completed_);
  DCHECK(data_length > 0);

  metadata_.assign(data, data_length);
}

void ResourceFetcher::didFinishLoading(
    WebURLLoader* loader, double finishTime) {
  DCHECK(!completed_);
  completed_ = true;

  RunCallback(response_, data_);
}

void ResourceFetcher::didFail(WebURLLoader* loader, const WebURLError& error) {
  DCHECK(!completed_);
  completed_ = true;

  // Go ahead and tell our delegate that we're done.
  RunCallback(WebURLResponse(), std::string());
}

/////////////////////////////////////////////////////////////////////////////
// A resource fetcher with a timeout

ResourceFetcherWithTimeout::ResourceFetcherWithTimeout(
    const GURL& url, WebFrame* frame, int timeout_secs, Callback* c)
    : ResourceFetcher(url, frame, c) {
  timeout_timer_.Start(TimeDelta::FromSeconds(timeout_secs), this,
                       &ResourceFetcherWithTimeout::TimeoutFired);
}

ResourceFetcherWithTimeout::~ResourceFetcherWithTimeout() {
}

void ResourceFetcherWithTimeout::TimeoutFired() {
  if (!completed_) {
    loader_->cancel();
    didFail(NULL, WebURLError());
  }
}

}  // namespace webkit_glue
