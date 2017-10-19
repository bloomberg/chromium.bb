// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/SubresourceFilter.h"

#include <utility>

#include "core/dom/Document.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/loader/DocumentLoader.h"
#include "core/workers/WorkerOrWorkletGlobalScope.h"
#include "platform/WebTaskRunner.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/text/StringBuilder.h"
#include "public/platform/WebTraceLocation.h"

namespace blink {

namespace {

String GetErrorStringForDisallowedLoad(const KURL& url) {
  StringBuilder builder;
  builder.Append("Chrome blocked resource ");
  builder.Append(url.GetString());
  builder.Append(
      " on this site because this site tends to show ads that interrupt, "
      "distract, or prevent user control. Learn more at "
      "https://www.chromestatus.com/feature/5738264052891648");
  return builder.ToString();
}

}  // namespace

// static
SubresourceFilter* SubresourceFilter::Create(
    ExecutionContext& execution_context,
    std::unique_ptr<WebDocumentSubresourceFilter> filter) {
  return new SubresourceFilter(&execution_context, std::move(filter));
}

SubresourceFilter::SubresourceFilter(
    ExecutionContext* execution_context,
    std::unique_ptr<WebDocumentSubresourceFilter> subresource_filter)
    : execution_context_(execution_context),
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
    ReportLoad(resource_url, load_policy);
  return load_policy != WebDocumentSubresourceFilter::kDisallow;
}

bool SubresourceFilter::AllowWebSocketConnection(const KURL& url) {
  // Currently WebSocket is handled via document on the main thread.
  DCHECK(execution_context_->IsDocument());

  WebDocumentSubresourceFilter::LoadPolicy load_policy =
      subresource_filter_->GetLoadPolicyForWebSocketConnect(url);

  // Post a task to notify this load to avoid unduly blocking the worker
  // thread. Note that this unconditionally calls reportLoad unlike allowLoad,
  // because there aren't developer-invisible connections (like speculative
  // preloads) happening here.
  scoped_refptr<WebTaskRunner> task_runner =
      TaskRunnerHelper::Get(TaskType::kNetworking, execution_context_);
  DCHECK(task_runner->RunsTasksInCurrentSequence());
  task_runner->PostTask(BLINK_FROM_HERE,
                        WTF::Bind(&SubresourceFilter::ReportLoad,
                                  WrapPersistent(this), url, load_policy));
  return load_policy != WebDocumentSubresourceFilter::kDisallow;
}

void SubresourceFilter::ReportLoad(
    const KURL& resource_url,
    WebDocumentSubresourceFilter::LoadPolicy load_policy) {
  switch (load_policy) {
    case WebDocumentSubresourceFilter::kAllow:
      break;
    case WebDocumentSubresourceFilter::kDisallow:
      subresource_filter_->ReportDisallowedLoad();

      // Display console message for actually blocked resource. For a
      // resource with |load_policy| as kWouldDisallow, we will be logging a
      // document wide console message, so no need to log it here.
      // TODO: Consider logging this as a kInterventionMessageSource for showing
      // warning in Lighthouse.
      if (subresource_filter_->ShouldLogToConsole()) {
        execution_context_->AddConsoleMessage(ConsoleMessage::Create(
            kOtherMessageSource, kErrorMessageLevel,
            GetErrorStringForDisallowedLoad(resource_url)));
      }
    // fall through
    case WebDocumentSubresourceFilter::kWouldDisallow:
      // TODO(csharrison): Consider posting a task to the main thread from
      // worker thread, or adding support for DidObserveLoadingBehavior to
      // ExecutionContext.
      if (execution_context_->IsDocument()) {
        if (DocumentLoader* loader = ToDocument(execution_context_)->Loader()) {
          loader->DidObserveLoadingBehavior(
              kWebLoadingBehaviorSubresourceFilterMatch);
        }
      }
      break;
  }
}

void SubresourceFilter::Trace(blink::Visitor* visitor) {
  visitor->Trace(execution_context_);
}

}  // namespace blink
