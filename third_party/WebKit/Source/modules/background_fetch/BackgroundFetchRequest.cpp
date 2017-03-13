// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/background_fetch/BackgroundFetchRequest.h"

#include "modules/fetch/Request.h"

namespace blink {

BackgroundFetchRequest::BackgroundFetchRequest(Request* request)
    : m_request(request) {}

Request* BackgroundFetchRequest::request() const {
  return m_request;
}

DEFINE_TRACE(BackgroundFetchRequest) {
  visitor->trace(m_request);
}

}  // namespace blink
