// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/SubresourceFilter.h"

#include "core/loader/DocumentLoader.h"
#include "public/platform/WebDocumentSubresourceFilter.h"

namespace blink {

// static
SubresourceFilter* SubresourceFilter::create(
    DocumentLoader* loader,
    std::unique_ptr<WebDocumentSubresourceFilter> filter) {
  return new SubresourceFilter(loader, std::move(filter));
}

SubresourceFilter::SubresourceFilter(
    DocumentLoader* documentLoader,
    std::unique_ptr<WebDocumentSubresourceFilter> subresourceFilter)
    : m_documentLoader(documentLoader),
      m_subresourceFilter(std::move(subresourceFilter)) {}

SubresourceFilter::~SubresourceFilter() {}

bool SubresourceFilter::allowLoad(
    const KURL& resourceUrl,
    WebURLRequest::RequestContext requestContext,
    SecurityViolationReportingPolicy reportingPolicy) {
  // TODO(csharrison): Implement a caching layer here which is a HashMap of
  // Pair<url string, context> -> LoadPolicy.
  WebDocumentSubresourceFilter::LoadPolicy loadPolicy =
      m_subresourceFilter->getLoadPolicy(resourceUrl, requestContext);
  if (reportingPolicy == SecurityViolationReportingPolicy::Report) {
    switch (loadPolicy) {
      case WebDocumentSubresourceFilter::Allow:
        break;
      case WebDocumentSubresourceFilter::Disallow:
        m_subresourceFilter->reportDisallowedLoad();
      // fall through
      case WebDocumentSubresourceFilter::WouldDisallow:
        m_documentLoader->didObserveLoadingBehavior(
            WebLoadingBehaviorSubresourceFilterMatch);
        break;
    }
  }
  return loadPolicy != WebDocumentSubresourceFilter::Disallow;
}

}  // namespace blink
