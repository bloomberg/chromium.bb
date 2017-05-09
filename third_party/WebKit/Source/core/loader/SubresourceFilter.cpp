// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/SubresourceFilter.h"

#include <utility>

#include "core/dom/TaskRunnerHelper.h"
#include "platform/WebTaskRunner.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/WebTraceLocation.h"

namespace blink {

// static
SubresourceFilter* SubresourceFilter::Create(
    DocumentLoader* loader,
    std::unique_ptr<WebDocumentSubresourceFilter> filter) {
  return new SubresourceFilter(loader, std::move(filter));
}

SubresourceFilter::SubresourceFilter(
    DocumentLoader* document_loader,
    std::unique_ptr<WebDocumentSubresourceFilter> subresource_filter)
    : document_loader_(document_loader),
      subresource_filter_(std::move(subresource_filter)) {}

SubresourceFilter::~SubresourceFilter() {}

bool SubresourceFilter::AllowLoad(
    const KURL& resource_url,
    WebURLRequest::RequestContext request_context,
    SecurityViolationReportingPolicy reporting_policy) {
  // TODO(csharrison): Implement a caching layer here which is a HashMap of
  // Pair<url string, context> -> LoadPolicy.
  WebDocumentSubresourceFilter::LoadPolicy load_policy =
      subresource_filter_->GetLoadPolicy(resource_url, request_context);
  if (reporting_policy == SecurityViolationReportingPolicy::kReport)
    ReportLoad(load_policy);
  return load_policy != WebDocumentSubresourceFilter::kDisallow;
}

bool SubresourceFilter::AllowWebSocketConnection(const KURL& url) {
  WebDocumentSubresourceFilter::LoadPolicy load_policy =
      subresource_filter_->GetLoadPolicyForWebSocketConnect(url);

  // Post a task to notify this load to avoid unduly blocking the worker
  // thread. Note that this unconditionally calls reportLoad unlike allowLoad,
  // because there aren't developer-invisible connections (like speculative
  // preloads) happening here.
  RefPtr<WebTaskRunner> task_runner = TaskRunnerHelper::Get(
      TaskType::kNetworking, document_loader_->GetFrame());
  DCHECK(task_runner->RunsTasksOnCurrentThread());
  task_runner->PostTask(BLINK_FROM_HERE,
                        WTF::Bind(&SubresourceFilter::ReportLoad,
                                  WrapPersistent(this), load_policy));
  return load_policy != WebDocumentSubresourceFilter::kDisallow;
}

void SubresourceFilter::ReportLoad(
    WebDocumentSubresourceFilter::LoadPolicy load_policy) {
  switch (load_policy) {
    case WebDocumentSubresourceFilter::kAllow:
      break;
    case WebDocumentSubresourceFilter::kDisallow:
      subresource_filter_->ReportDisallowedLoad();
    // fall through
    case WebDocumentSubresourceFilter::kWouldDisallow:
      // TODO(csharrison): log console errors here based on
      // subresource_filter_->ShouldLogToConsole().
      document_loader_->DidObserveLoadingBehavior(
          kWebLoadingBehaviorSubresourceFilterMatch);
      break;
  }
}

}  // namespace blink
