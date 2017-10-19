// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/background_fetch/BackgroundFetchFetch.h"

#include "modules/fetch/Request.h"

namespace blink {

BackgroundFetchFetch::BackgroundFetchFetch(Request* request)
    : request_(request) {}

Request* BackgroundFetchFetch::request() const {
  return request_;
}

void BackgroundFetchFetch::Trace(blink::Visitor* visitor) {
  visitor->Trace(request_);
}

}  // namespace blink
