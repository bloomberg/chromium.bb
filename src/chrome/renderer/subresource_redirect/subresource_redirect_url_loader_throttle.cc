// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/subresource_redirect/subresource_redirect_url_loader_throttle.h"

#include "chrome/renderer/subresource_redirect/subresource_redirect_util.h"
#include "content/public/common/resource_type.h"
#include "url/gurl.h"

namespace subresource_redirect {

SubresourceRedirectURLLoaderThrottle::SubresourceRedirectURLLoaderThrottle() =
    default;
SubresourceRedirectURLLoaderThrottle::~SubresourceRedirectURLLoaderThrottle() =
    default;

void SubresourceRedirectURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  if (request->resource_type != static_cast<int>(content::ResourceType::kImage))
    return;

  if (!request->url.SchemeIs(url::kHttpsScheme))
    return;

  request->url = GetSubresourceURLForURL(request->url);
  *defer = false;
}

void SubresourceRedirectURLLoaderThrottle::DetachFromCurrentSequence() {}

}  // namespace subresource_redirect