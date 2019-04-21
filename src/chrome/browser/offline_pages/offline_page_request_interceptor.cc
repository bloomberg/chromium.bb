// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/offline_page_request_interceptor.h"

#include "base/supports_user_data.h"
#include "chrome/browser/offline_pages/offline_page_request_job.h"

namespace offline_pages {

OfflinePageRequestInterceptor::OfflinePageRequestInterceptor() = default;

OfflinePageRequestInterceptor::~OfflinePageRequestInterceptor() = default;

net::URLRequestJob* OfflinePageRequestInterceptor::MaybeInterceptRequest(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) const {
  // OfflinePageRequestJob::Create may return a nullptr if the interception
  // is not needed for some sort of requests, like non-main resource request,
  // non-http request and more.
  return OfflinePageRequestJob::Create(request, network_delegate);
}

}  // namespace offline_pages
