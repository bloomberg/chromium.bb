// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/WebSharedWorkerReportingProxyImpl.h"

#include "bindings/core/v8/SourceLocation.h"
#include "platform/CrossThreadFunctional.h"
#include "public/platform/WebTraceLocation.h"
#include "web/WebSharedWorkerImpl.h"
#include "wtf/WTF.h"

namespace blink {

WebSharedWorkerReportingProxyImpl::WebSharedWorkerReportingProxyImpl(
    WebSharedWorkerImpl* worker,
    ParentFrameTaskRunners* parentFrameTaskRunners)
    : m_worker(worker), m_parentFrameTaskRunners(parentFrameTaskRunners) {
  DCHECK(isMainThread());
}

WebSharedWorkerReportingProxyImpl::~WebSharedWorkerReportingProxyImpl() {
  DCHECK(isMainThread());
}

void WebSharedWorkerReportingProxyImpl::countFeature(
    UseCounter::Feature feature) {
  DCHECK(!isMainThread());
  m_parentFrameTaskRunners->get(TaskType::UnspecedTimer)
      ->postTask(BLINK_FROM_HERE,
                 crossThreadBind(&WebSharedWorkerImpl::countFeature,
                                 crossThreadUnretained(m_worker), feature));
}

void WebSharedWorkerReportingProxyImpl::countDeprecation(
    UseCounter::Feature feature) {
  DCHECK(!isMainThread());
  // Go through the same code path with countFeature() because a deprecation
  // message is already shown on the worker console and a remaining work is just
  // to record an API use.
  countFeature(feature);
}

void WebSharedWorkerReportingProxyImpl::reportException(
    const String& errorMessage,
    std::unique_ptr<SourceLocation>,
    int exceptionId) {
  DCHECK(!isMainThread());
  // Not suppported in SharedWorker.
}

void WebSharedWorkerReportingProxyImpl::reportConsoleMessage(
    MessageSource,
    MessageLevel,
    const String& message,
    SourceLocation*) {
  DCHECK(!isMainThread());
  // Not supported in SharedWorker.
}

void WebSharedWorkerReportingProxyImpl::postMessageToPageInspector(
    const String& message) {
  DCHECK(!isMainThread());
  // The TaskType of Inspector tasks need to be Unthrottled because they need to
  // run even on a suspended page.
  m_parentFrameTaskRunners->get(TaskType::Unthrottled)
      ->postTask(
          BLINK_FROM_HERE,
          crossThreadBind(&WebSharedWorkerImpl::postMessageToPageInspector,
                          crossThreadUnretained(m_worker), message));
}

void WebSharedWorkerReportingProxyImpl::didCloseWorkerGlobalScope() {
  DCHECK(!isMainThread());
  m_parentFrameTaskRunners->get(TaskType::UnspecedTimer)
      ->postTask(
          BLINK_FROM_HERE,
          crossThreadBind(&WebSharedWorkerImpl::didCloseWorkerGlobalScope,
                          crossThreadUnretained(m_worker)));
}

void WebSharedWorkerReportingProxyImpl::didTerminateWorkerThread() {
  DCHECK(!isMainThread());
  m_parentFrameTaskRunners->get(TaskType::UnspecedTimer)
      ->postTask(BLINK_FROM_HERE,
                 crossThreadBind(&WebSharedWorkerImpl::didTerminateWorkerThread,
                                 crossThreadUnretained(m_worker)));
}

DEFINE_TRACE(WebSharedWorkerReportingProxyImpl) {
  visitor->trace(m_parentFrameTaskRunners);
}

}  // namespace blink
