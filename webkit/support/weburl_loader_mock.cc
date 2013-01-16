// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/support/weburl_loader_mock.h"

#include "base/logging.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebData.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURLError.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURLLoaderClient.h"
#include "webkit/support/weburl_loader_mock_factory.h"

WebURLLoaderMock::WebURLLoaderMock(WebURLLoaderMockFactory* factory,
                                   WebKit::WebURLLoader* default_loader)
    : factory_(factory),
      client_(NULL),
      default_loader_(default_loader),
      using_default_loader_(false),
      is_deferred_(false) {
}

WebURLLoaderMock::~WebURLLoaderMock() {
}

void WebURLLoaderMock::ServeAsynchronousRequest(
    const WebKit::WebURLResponse& response,
    const WebKit::WebData& data,
    const WebKit::WebURLError& error) {
  DCHECK(!using_default_loader_);
  if (!client_)
    return;

  client_->didReceiveResponse(this, response);

  if (error.reason) {
    client_->didFail(this, error);
    return;
  }
  client_->didReceiveData(this, data.data(), data.size(), data.size());
  client_->didFinishLoading(this, 0);
}

WebKit::WebURLRequest WebURLLoaderMock::ServeRedirect(
    const WebKit::WebURLResponse& redirectResponse) {
  WebKit::WebURLRequest newRequest;
  newRequest.initialize();
  GURL redirectURL(redirectResponse.httpHeaderField("Location"));
  newRequest.setURL(redirectURL);
  client_->willSendRequest(this, newRequest, redirectResponse);
  return newRequest;
}

void WebURLLoaderMock::loadSynchronously(const WebKit::WebURLRequest& request,
                                         WebKit::WebURLResponse& response,
                                         WebKit::WebURLError& error,
                                         WebKit::WebData& data) {
  if (factory_->IsMockedURL(request.url())) {
    factory_->LoadSynchronously(request, &response, &error, &data);
    return;
  }
  using_default_loader_ = true;
  default_loader_->loadSynchronously(request, response, error, data);
}

void WebURLLoaderMock::loadAsynchronously(const WebKit::WebURLRequest& request,
                                          WebKit::WebURLLoaderClient* client) {
  if (factory_->IsMockedURL(request.url())) {
    client_ = client;
    factory_->LoadAsynchronouly(request, this);
    return;
  }
  using_default_loader_ = true;
  default_loader_->loadAsynchronously(request, client);
}

void WebURLLoaderMock::cancel() {
  if (using_default_loader_) {
    default_loader_->cancel();
    return;
  }
  client_ = NULL;
  factory_->CancelLoad(this);
}

void WebURLLoaderMock::setDefersLoading(bool deferred) {
  is_deferred_ = deferred;
  if (using_default_loader_) {
    default_loader_->setDefersLoading(deferred);
    return;
  }
  NOTIMPLEMENTED();
}
