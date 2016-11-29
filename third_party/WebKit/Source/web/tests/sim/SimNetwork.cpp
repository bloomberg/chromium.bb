// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/tests/sim/SimNetwork.h"

#include "public/platform/Platform.h"
#include "public/platform/WebURLError.h"
#include "public/platform/WebURLLoader.h"
#include "public/platform/WebURLLoaderClient.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/platform/WebURLResponse.h"
#include "web/tests/sim/SimRequest.h"

namespace blink {

static SimNetwork* s_network = nullptr;

SimNetwork::SimNetwork() : m_currentRequest(nullptr) {
  Platform::current()->getURLLoaderMockFactory()->setLoaderDelegate(this);
  ASSERT(!s_network);
  s_network = this;
}

SimNetwork::~SimNetwork() {
  Platform::current()->getURLLoaderMockFactory()->setLoaderDelegate(nullptr);
  Platform::current()->getURLLoaderMockFactory()->unregisterAllURLs();
  s_network = nullptr;
}

SimNetwork& SimNetwork::current() {
  DCHECK(s_network);
  return *s_network;
}

void SimNetwork::servePendingRequests() {
  Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();
}

void SimNetwork::didReceiveResponse(WebURLLoaderClient* client,
                                    const WebURLResponse& response) {
  auto it = m_requests.find(response.url().string());
  if (it == m_requests.end()) {
    client->didReceiveResponse(response);
    return;
  }
  DCHECK(it->value);
  m_currentRequest = it->value;
  m_currentRequest->didReceiveResponse(client, response);
}

void SimNetwork::didReceiveData(WebURLLoaderClient* client,
                                const char* data,
                                int dataLength,
                                int encodedDataLength) {
  if (!m_currentRequest)
    client->didReceiveData(data, dataLength, encodedDataLength);
}

void SimNetwork::didFail(WebURLLoaderClient* client,
                         const WebURLError& error,
                         int64_t totalEncodedDataLength,
                         int64_t totalEncodedBodyLength) {
  if (!m_currentRequest) {
    client->didFail(error, totalEncodedDataLength, totalEncodedBodyLength);
    return;
  }
  m_currentRequest->didFail(error);
}

void SimNetwork::didFinishLoading(WebURLLoaderClient* client,
                                  double finishTime,
                                  int64_t totalEncodedDataLength,
                                  int64_t totalEncodedBodyLength) {
  if (!m_currentRequest) {
    client->didFinishLoading(finishTime, totalEncodedDataLength,
                             totalEncodedBodyLength);
    return;
  }
  m_currentRequest = nullptr;
}

void SimNetwork::addRequest(SimRequest& request) {
  m_requests.add(request.url(), &request);
}

void SimNetwork::removeRequest(SimRequest& request) {
  m_requests.remove(request.url());
}

}  // namespace blink
