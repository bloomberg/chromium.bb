// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/background_fetch/BackgroundFetchFetch.h"

#include "modules/fetch/Request.h"

namespace blink {

BackgroundFetchFetch::BackgroundFetchFetch(Request* request)
    : m_request(request) {}

Request* BackgroundFetchFetch::request() const {
  return m_request;
}

DEFINE_TRACE(BackgroundFetchFetch) {
  visitor->trace(m_request);
}

}  // namespace blink
