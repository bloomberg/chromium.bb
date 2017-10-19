// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/testing/sim/SimRequest.h"

#include "core/testing/sim/SimNetwork.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURLLoaderClient.h"
#include "public/platform/WebURLLoaderMockFactory.h"

namespace blink {

SimRequest::SimRequest(String url, String mime_type)
    : url_(url),
      client_(nullptr),
      total_encoded_data_length_(0),
      is_ready_(false) {
  KURL full_url(url);
  WebURLResponse response(full_url);
  response.SetMIMEType(mime_type);
  response.SetHTTPStatusCode(200);
  Platform::Current()->GetURLLoaderMockFactory()->RegisterURL(full_url,
                                                              response, "");
  SimNetwork::Current().AddRequest(*this);
}

SimRequest::~SimRequest() {
  DCHECK(!is_ready_);
}

void SimRequest::DidReceiveResponse(WebURLLoaderClient* client,
                                    const WebURLResponse& response) {
  client_ = client;
  response_ = response;
  is_ready_ = true;
}

void SimRequest::DidFail(const WebURLError& error) {
  error_ = error;
}

void SimRequest::Start() {
  SimNetwork::Current().ServePendingRequests();
  DCHECK(is_ready_);
  client_->DidReceiveResponse(response_);
}

void SimRequest::Write(const String& data) {
  DCHECK(is_ready_);
  DCHECK(!error_.reason);
  total_encoded_data_length_ += data.length();
  client_->DidReceiveData(data.Utf8().data(), data.length());
}

void SimRequest::Write(const Vector<char>& data) {
  DCHECK(is_ready_);
  DCHECK(!error_.reason);
  total_encoded_data_length_ += data.size();
  client_->DidReceiveData(data.data(), data.size());
}

void SimRequest::Finish() {
  DCHECK(is_ready_);
  if (error_.reason) {
    client_->DidFail(error_, total_encoded_data_length_,
                     total_encoded_data_length_, total_encoded_data_length_);
  } else {
    // TODO(esprehn): Is claiming a request time of 0 okay for tests?
    client_->DidFinishLoading(0, total_encoded_data_length_,
                              total_encoded_data_length_,
                              total_encoded_data_length_);
  }
  Reset();
}

void SimRequest::Complete(const String& data) {
  Start();
  if (!data.IsEmpty())
    Write(data);
  Finish();
}

void SimRequest::Complete(const Vector<char>& data) {
  Start();
  if (!data.IsEmpty())
    Write(data);
  Finish();
}

void SimRequest::Reset() {
  is_ready_ = false;
  client_ = nullptr;
  SimNetwork::Current().RemoveRequest(*this);
}

}  // namespace blink
