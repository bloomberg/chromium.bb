// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/sim/sim_request.h"

#include "third_party/blink/public/platform/web_url_loader_client.h"
#include "third_party/blink/renderer/core/testing/sim/sim_network.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

SimRequestBase::SimRequestBase(String url,
                               String mime_type,
                               bool start_immediately)
    : url_(url),
      mime_type_(mime_type),
      start_immediately_(start_immediately),
      started_(false),
      client_(nullptr),
      total_encoded_data_length_(0) {
  SimNetwork::Current().AddRequest(*this);
}

SimRequestBase::~SimRequestBase() {
  DCHECK(!client_);
}

void SimRequestBase::DidReceiveResponse(WebURLLoaderClient* client,
                                        const WebURLResponse& response) {
  client_ = client;
  response_ = response;
  started_ = false;
  if (start_immediately_)
    StartInternal();
}

void SimRequestBase::DidFail(const WebURLError& error) {
  error_ = error;
}

void SimRequestBase::StartInternal() {
  DCHECK(!started_);
  started_ = true;
  client_->DidReceiveResponse(response_);
}

void SimRequestBase::Write(const String& data) {
  if (!started_)
    ServePending();
  DCHECK(started_);
  DCHECK(!error_);
  total_encoded_data_length_ += data.length();
  client_->DidReceiveData(data.Utf8().data(), data.length());
}

void SimRequestBase::Write(const Vector<char>& data) {
  if (!started_)
    ServePending();
  DCHECK(started_);
  DCHECK(!error_);
  total_encoded_data_length_ += data.size();
  client_->DidReceiveData(data.data(), data.size());
}

void SimRequestBase::Finish() {
  if (!started_)
    ServePending();
  DCHECK(started_);
  if (error_) {
    client_->DidFail(*error_, total_encoded_data_length_,
                     total_encoded_data_length_, total_encoded_data_length_);
  } else {
    // TODO(esprehn): Is claiming a request time of 0 okay for tests?
    client_->DidFinishLoading(
        TimeTicks(), total_encoded_data_length_, total_encoded_data_length_,
        total_encoded_data_length_, false,
        std::vector<network::cors::PreflightTimingInfo>());
  }
  Reset();
}

void SimRequestBase::Complete(const String& data) {
  if (!started_)
    ServePending();
  if (!started_)
    StartInternal();
  if (!data.IsEmpty())
    Write(data);
  Finish();
}

void SimRequestBase::Complete(const Vector<char>& data) {
  if (!started_)
    ServePending();
  if (!started_)
    StartInternal();
  if (!data.IsEmpty())
    Write(data);
  Finish();
}

void SimRequestBase::Reset() {
  started_ = false;
  client_ = nullptr;
  SimNetwork::Current().RemoveRequest(*this);
}

void SimRequestBase::ServePending() {
  SimNetwork::Current().ServePendingRequests();
}

SimRequest::SimRequest(String url, String mime_type)
    : SimRequestBase(url, mime_type, true /* start_immediately */) {}

SimRequest::~SimRequest() = default;

SimSubresourceRequest::SimSubresourceRequest(String url, String mime_type)
    : SimRequestBase(url, mime_type, false /* start_immediately */) {}

SimSubresourceRequest::~SimSubresourceRequest() = default;

void SimSubresourceRequest::Start() {
  ServePending();
  StartInternal();
}

}  // namespace blink
