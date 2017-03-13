// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/background_fetch/BackgroundFetchSettledRequest.h"

#include "modules/fetch/Response.h"

namespace blink {

BackgroundFetchSettledRequest::BackgroundFetchSettledRequest(Request* request,
                                                             Response* response)
    : BackgroundFetchRequest(request), m_response(response) {}

Response* BackgroundFetchSettledRequest::response() const {
  return m_response;
}

DEFINE_TRACE(BackgroundFetchSettledRequest) {
  visitor->trace(m_response);
  BackgroundFetchRequest::trace(visitor);
}

}  // namespace blink
