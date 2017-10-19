// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/loader/testing/MockResourceClient.h"

namespace blink {

MockResourceClient::MockResourceClient(Resource* resource)
    : resource_(resource),
      notify_finished_called_(false),
      encoded_size_on_notify_finished_(0) {
  resource_->AddClient(this);
}

MockResourceClient::~MockResourceClient() {}

void MockResourceClient::NotifyFinished(Resource* resource) {
  CHECK(!notify_finished_called_);
  notify_finished_called_ = true;
  encoded_size_on_notify_finished_ = resource->EncodedSize();
}

void MockResourceClient::RemoveAsClient() {
  resource_->RemoveClient(this);
  resource_ = nullptr;
}

void MockResourceClient::Dispose() {
  if (resource_) {
    resource_->RemoveClient(this);
    resource_ = nullptr;
  }
}

void MockResourceClient::Trace(blink::Visitor* visitor) {
  visitor->Trace(resource_);
  ResourceClient::Trace(visitor);
}

}  // namespace blink
