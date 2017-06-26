// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/modules/serviceworker/WebServiceWorkerStreamHandle.h"

namespace blink {

void WebServiceWorkerStreamHandle::SetListener(
    std::unique_ptr<Listener> listener) {
  DCHECK(!listener_);
  listener_ = std::move(listener);
}

void WebServiceWorkerStreamHandle::Aborted() {
  DCHECK(listener_);
  listener_->OnAborted();
}

void WebServiceWorkerStreamHandle::Completed() {
  DCHECK(listener_);
  listener_->OnCompleted();
}

mojo::ScopedDataPipeConsumerHandle
WebServiceWorkerStreamHandle::DrainStreamDataPipe() {
  // Temporary CHECK to debug https://crbug.com/734978.
  CHECK(stream_.is_valid());
  return std::move(stream_);
}

}  // namespace blink
