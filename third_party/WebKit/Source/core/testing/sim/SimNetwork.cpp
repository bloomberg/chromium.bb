// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/testing/sim/SimNetwork.h"

#include "core/testing/sim/SimRequest.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURLError.h"
#include "public/platform/WebURLLoader.h"
#include "public/platform/WebURLLoaderClient.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/platform/WebURLResponse.h"

namespace blink {

static SimNetwork* g_network = nullptr;

SimNetwork::SimNetwork() : current_request_(nullptr) {
  Platform::Current()->GetURLLoaderMockFactory()->SetLoaderDelegate(this);
  DCHECK(!g_network);
  g_network = this;
}

SimNetwork::~SimNetwork() {
  Platform::Current()->GetURLLoaderMockFactory()->SetLoaderDelegate(nullptr);
  Platform::Current()
      ->GetURLLoaderMockFactory()
      ->UnregisterAllURLsAndClearMemoryCache();
  g_network = nullptr;
}

SimNetwork& SimNetwork::Current() {
  DCHECK(g_network);
  return *g_network;
}

void SimNetwork::ServePendingRequests() {
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
}

void SimNetwork::DidReceiveResponse(WebURLLoaderClient* client,
                                    const WebURLResponse& response) {
  auto it = requests_.find(response.Url().GetString());
  if (it == requests_.end()) {
    client->DidReceiveResponse(response);
    return;
  }
  DCHECK(it->value);
  current_request_ = it->value;
  current_request_->DidReceiveResponse(client, response);
}

void SimNetwork::DidReceiveData(WebURLLoaderClient* client,
                                const char* data,
                                int data_length) {
  if (!current_request_)
    client->DidReceiveData(data, data_length);
}

void SimNetwork::DidFail(WebURLLoaderClient* client,
                         const WebURLError& error,
                         int64_t total_encoded_data_length,
                         int64_t total_encoded_body_length,
                         int64_t total_decoded_body_length) {
  if (!current_request_) {
    client->DidFail(error, total_encoded_data_length, total_encoded_body_length,
                    total_decoded_body_length);
    return;
  }
  current_request_->DidFail(error);
}

void SimNetwork::DidFinishLoading(WebURLLoaderClient* client,
                                  double finish_time,
                                  int64_t total_encoded_data_length,
                                  int64_t total_encoded_body_length,
                                  int64_t total_decoded_body_length) {
  if (!current_request_) {
    client->DidFinishLoading(finish_time, total_encoded_data_length,
                             total_encoded_body_length,
                             total_decoded_body_length);
    return;
  }
  current_request_ = nullptr;
}

void SimNetwork::AddRequest(SimRequest& request) {
  requests_.insert(request.Url(), &request);
}

void SimNetwork::RemoveRequest(SimRequest& request) {
  requests_.erase(request.Url());
}

}  // namespace blink
