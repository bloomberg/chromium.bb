// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/resource/LinkFetchResource.h"

#include "platform/loader/fetch/FetchParameters.h"
#include "platform/loader/fetch/ResourceFetcher.h"

namespace blink {

Resource* LinkFetchResource::Fetch(Resource::Type type,
                                   FetchParameters& params,
                                   ResourceFetcher* fetcher) {
  DCHECK_EQ(type, kLinkPrefetch);
  DCHECK_EQ(params.GetResourceRequest().GetFrameType(),
            WebURLRequest::kFrameTypeNone);
  return fetcher->RequestResource(params, LinkResourceFactory(type));
}

LinkFetchResource::LinkFetchResource(const ResourceRequest& request,
                                     Type type,
                                     const ResourceLoaderOptions& options)
    : Resource(request, type, options) {}

LinkFetchResource::~LinkFetchResource() {}

}  // namespace blink
