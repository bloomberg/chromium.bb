// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebBackgroundFetchSettledFetch_h
#define WebBackgroundFetchSettledFetch_h

#include "public/platform/WebCommon.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerRequest.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerResponse.h"

namespace blink {

// Represents a request/response pair for a settled Background Fetch.
// Analogous to the following structure in the spec:
// http://wicg.github.io/background-fetch/#backgroundfetchsettledfetch
struct WebBackgroundFetchSettledFetch {
  WebBackgroundFetchSettledFetch() = default;
  ~WebBackgroundFetchSettledFetch() = default;

  WebServiceWorkerRequest request;
  WebServiceWorkerResponse response;
};

}  // namespace blink

#endif  // WebBackgroundFetchSettledFetch_h
