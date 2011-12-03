// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/resource_fetcher.h"

#include "base/logging.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebKitPlatformSupport.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURLError.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURLLoader.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURL.h"

using base::TimeDelta;
using WebKit::WebFrame;
using WebKit::WebURLError;
using WebKit::WebURLLoader;
using WebKit::WebURLRequest;
using WebKit::WebURLResponse;

namespace webkit_glue {

ResourceFetcher::ResourceFetcher(const GURL& url, WebFrame* frame,
                                 WebURLRequest::TargetType target_type,
                                 const Callback& callback)
    : url_(url),
      target_type_(target_type),
      completed_(false),
      callback_(callback) {
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
  request.setTargetType(target_type_);
  frame->dispatchWillSendRequest(request);

  loader_.reset(WebKit::webKitPlatformSupport()->createURLLoader());
  loader_->loadAsynchronously(request, this);
}

void ResourceFetcher::RunCallback(const WebURLResponse& response,
                                  const std::string& data) {
  if (callback_.is_null())
    return;

  // Take a reference to the callback as running the callback may lead to our
  // destruction.
  Callback callback = callback_;
  callback.Run(response, data);
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
    WebURLLoader* loader, const char* data, int data_length,
    int encoded_data_length) {
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
    const GURL& url, WebFrame* frame, WebURLRequest::TargetType target_type,
    int timeout_secs, const Callback& callback)
    : ResourceFetcher(url, frame, target_type, callback) {
  timeout_timer_.Start(FROM_HERE, TimeDelta::FromSeconds(timeout_secs), this,
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
