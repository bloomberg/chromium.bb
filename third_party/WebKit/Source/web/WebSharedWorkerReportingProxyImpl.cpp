// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/WebSharedWorkerReportingProxyImpl.h"

#include "bindings/core/v8/SourceLocation.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/wtf/WTF.h"
#include "public/platform/WebTraceLocation.h"
#include "web/WebSharedWorkerImpl.h"

namespace blink {

WebSharedWorkerReportingProxyImpl::WebSharedWorkerReportingProxyImpl(
    WebSharedWorkerImpl* worker,
    ParentFrameTaskRunners* parent_frame_task_runners)
    : worker_(worker), parent_frame_task_runners_(parent_frame_task_runners) {
  DCHECK(IsMainThread());
}

WebSharedWorkerReportingProxyImpl::~WebSharedWorkerReportingProxyImpl() {
  DCHECK(IsMainThread());
}

void WebSharedWorkerReportingProxyImpl::CountFeature(
    UseCounter::Feature feature) {
  DCHECK(!IsMainThread());
  parent_frame_task_runners_->Get(TaskType::kUnspecedTimer)
      ->PostTask(BLINK_FROM_HERE,
                 CrossThreadBind(&WebSharedWorkerImpl::CountFeature,
                                 CrossThreadUnretained(worker_), feature));
}

void WebSharedWorkerReportingProxyImpl::CountDeprecation(
    UseCounter::Feature feature) {
  DCHECK(!IsMainThread());
  // Go through the same code path with countFeature() because a deprecation
  // message is already shown on the worker console and a remaining work is just
  // to record an API use.
  CountFeature(feature);
}

void WebSharedWorkerReportingProxyImpl::ReportException(
    const String& error_message,
    std::unique_ptr<SourceLocation>,
    int exception_id) {
  DCHECK(!IsMainThread());
  // Not suppported in SharedWorker.
}

void WebSharedWorkerReportingProxyImpl::ReportConsoleMessage(
    MessageSource,
    MessageLevel,
    const String& message,
    SourceLocation*) {
  DCHECK(!IsMainThread());
  // Not supported in SharedWorker.
}

void WebSharedWorkerReportingProxyImpl::PostMessageToPageInspector(
    const String& message) {
  DCHECK(!IsMainThread());
  // The TaskType of Inspector tasks need to be Unthrottled because they need to
  // run even on a suspended page.
  parent_frame_task_runners_->Get(TaskType::kUnthrottled)
      ->PostTask(
          BLINK_FROM_HERE,
          CrossThreadBind(&WebSharedWorkerImpl::PostMessageToPageInspector,
                          CrossThreadUnretained(worker_), message));
}

void WebSharedWorkerReportingProxyImpl::DidCloseWorkerGlobalScope() {
  DCHECK(!IsMainThread());
  parent_frame_task_runners_->Get(TaskType::kUnspecedTimer)
      ->PostTask(
          BLINK_FROM_HERE,
          CrossThreadBind(&WebSharedWorkerImpl::DidCloseWorkerGlobalScope,
                          CrossThreadUnretained(worker_)));
}

void WebSharedWorkerReportingProxyImpl::DidTerminateWorkerThread() {
  DCHECK(!IsMainThread());
  parent_frame_task_runners_->Get(TaskType::kUnspecedTimer)
      ->PostTask(BLINK_FROM_HERE,
                 CrossThreadBind(&WebSharedWorkerImpl::DidTerminateWorkerThread,
                                 CrossThreadUnretained(worker_)));
}

DEFINE_TRACE(WebSharedWorkerReportingProxyImpl) {
  visitor->Trace(parent_frame_task_runners_);
}

}  // namespace blink
