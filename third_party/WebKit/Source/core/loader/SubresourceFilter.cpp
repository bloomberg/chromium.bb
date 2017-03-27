// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/SubresourceFilter.h"

#include "core/dom/TaskRunnerHelper.h"
#include "core/loader/DocumentLoader.h"
#include "platform/WebTaskRunner.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/WebTraceLocation.h"

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
  if (reportingPolicy == SecurityViolationReportingPolicy::Report)
    reportLoad(loadPolicy);
  return loadPolicy != WebDocumentSubresourceFilter::kDisallow;
}

bool SubresourceFilter::allowWebSocketConnection(const KURL& url) {
  WebDocumentSubresourceFilter::LoadPolicy loadPolicy =
      m_subresourceFilter->getLoadPolicyForWebSocketConnect(url);

  // Post a task to notify this load to avoid unduly blocking the worker
  // thread. Note that this unconditionally calls reportLoad unlike allowLoad,
  // because there aren't developer-invisible connections (like speculative
  // preloads) happening here.
  RefPtr<WebTaskRunner> taskRunner =
      TaskRunnerHelper::get(TaskType::Networking, m_documentLoader->frame());
  DCHECK(taskRunner->runsTasksOnCurrentThread());
  taskRunner->postTask(BLINK_FROM_HERE,
                       WTF::bind(&SubresourceFilter::reportLoad,
                                 wrapPersistent(this), loadPolicy));
  return loadPolicy != WebDocumentSubresourceFilter::kDisallow;
}

void SubresourceFilter::reportLoad(
    WebDocumentSubresourceFilter::LoadPolicy loadPolicy) {
  // TODO(csharrison): log console errors here.
  switch (loadPolicy) {
    case WebDocumentSubresourceFilter::kAllow:
      break;
    case WebDocumentSubresourceFilter::kDisallow:
      m_subresourceFilter->reportDisallowedLoad();
    // fall through
    case WebDocumentSubresourceFilter::kWouldDisallow:
      m_documentLoader->didObserveLoadingBehavior(
          WebLoadingBehaviorSubresourceFilterMatch);
      break;
  }
}

}  // namespace blink
