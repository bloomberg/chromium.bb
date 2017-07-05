// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/ThreadedMessagingProxyBase.h"

#include "bindings/core/v8/SourceLocation.h"
#include "core/dom/Document.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/frame/Deprecation.h"
#include "core/frame/WebLocalFrameBase.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/ThreadableLoadingContext.h"
#include "core/loader/WorkerFetchContext.h"
#include "core/workers/WorkerInspectorProxy.h"
#include "core/workers/WorkerThreadStartupData.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/wtf/CurrentTime.h"
#include "public/platform/WebWorkerFetchContext.h"
#include "public/web/WebFrameClient.h"

namespace blink {

namespace {

static int g_live_messaging_proxy_count = 0;

}  // namespace

ThreadedMessagingProxyBase::ThreadedMessagingProxyBase(
    ExecutionContext* execution_context,
    WorkerClients* worker_clients)
    : execution_context_(execution_context),
      worker_clients_(worker_clients),
      worker_inspector_proxy_(WorkerInspectorProxy::Create()),
      parent_frame_task_runners_(ParentFrameTaskRunners::Create(
          *ToDocument(execution_context_.Get())->GetFrame())),
      asked_to_terminate_(false),
      keep_alive_(this) {
  DCHECK(IsParentContextThread());
  g_live_messaging_proxy_count++;

  if (RuntimeEnabledFeatures::OffMainThreadFetchEnabled()) {
    Document* document = ToDocument(execution_context_);
    WebLocalFrameBase* web_frame =
        WebLocalFrameBase::FromFrame(document->GetFrame());
    std::unique_ptr<WebWorkerFetchContext> web_worker_fetch_context =
        web_frame->Client()->CreateWorkerFetchContext();
    DCHECK(web_worker_fetch_context);
    web_worker_fetch_context->SetApplicationCacheHostID(
        document->Fetcher()->Context().ApplicationCacheHostID());
    web_worker_fetch_context->SetDataSaverEnabled(
        document->GetFrame()->GetSettings()->GetDataSaverEnabled());
    ProvideWorkerFetchContextToWorker(worker_clients,
                                      std::move(web_worker_fetch_context));
  }
}

ThreadedMessagingProxyBase::~ThreadedMessagingProxyBase() {
  g_live_messaging_proxy_count--;
}

int ThreadedMessagingProxyBase::ProxyCount() {
  DCHECK(IsMainThread());
  return g_live_messaging_proxy_count;
}

DEFINE_TRACE(ThreadedMessagingProxyBase) {
  visitor->Trace(execution_context_);
  visitor->Trace(worker_clients_);
  visitor->Trace(worker_inspector_proxy_);
}

void ThreadedMessagingProxyBase::InitializeWorkerThread(
    std::unique_ptr<WorkerThreadStartupData> startup_data,
    const KURL& script_url) {
  DCHECK(IsParentContextThread());

  Document* document = ToDocument(GetExecutionContext());
  double origin_time =
      document->Loader()
          ? document->Loader()->GetTiming().ReferenceMonotonicTime()
          : MonotonicallyIncreasingTime();

  worker_thread_ = CreateWorkerThread(origin_time);
  worker_thread_->Start(std::move(startup_data), GetParentFrameTaskRunners());
  WorkerThreadCreated();
  GetWorkerInspectorProxy()->WorkerThreadCreated(document, GetWorkerThread(),
                                                 script_url);
}

void ThreadedMessagingProxyBase::CountFeature(WebFeature feature) {
  DCHECK(IsParentContextThread());
  UseCounter::Count(execution_context_, feature);
}

void ThreadedMessagingProxyBase::CountDeprecation(WebFeature feature) {
  DCHECK(IsParentContextThread());
  Deprecation::CountDeprecation(execution_context_, feature);
}

void ThreadedMessagingProxyBase::ReportConsoleMessage(
    MessageSource source,
    MessageLevel level,
    const String& message,
    std::unique_ptr<SourceLocation> location) {
  DCHECK(IsParentContextThread());
  if (asked_to_terminate_)
    return;
  if (worker_inspector_proxy_)
    worker_inspector_proxy_->AddConsoleMessageFromWorker(level, message,
                                                         std::move(location));
}

void ThreadedMessagingProxyBase::WorkerThreadCreated() {
  DCHECK(IsParentContextThread());
  DCHECK(!asked_to_terminate_);
  DCHECK(worker_thread_);
}

void ThreadedMessagingProxyBase::ParentObjectDestroyed() {
  DCHECK(IsParentContextThread());
  if (worker_thread_) {
    // Request to terminate the global scope. This will eventually call
    // WorkerThreadTerminated().
    TerminateGlobalScope();
  } else {
    WorkerThreadTerminated();
  }
}

void ThreadedMessagingProxyBase::WorkerThreadTerminated() {
  DCHECK(IsParentContextThread());

  // This method is always the last to be performed, so the proxy is not
  // needed for communication in either side any more. However, the parent
  // Worker/Worklet object may still exist, and it assumes that the proxy
  // exists, too.
  asked_to_terminate_ = true;
  worker_thread_ = nullptr;
  worker_inspector_proxy_->WorkerThreadTerminated();

  // If the parent Worker/Worklet object was already destroyed, this will
  // destroy |this|.
  keep_alive_.Clear();
}

void ThreadedMessagingProxyBase::TerminateGlobalScope() {
  DCHECK(IsParentContextThread());

  if (asked_to_terminate_)
    return;
  asked_to_terminate_ = true;

  if (worker_thread_)
    worker_thread_->Terminate();

  worker_inspector_proxy_->WorkerThreadTerminated();
}

void ThreadedMessagingProxyBase::PostMessageToPageInspector(
    int session_id,
    const String& message) {
  DCHECK(IsParentContextThread());
  if (worker_inspector_proxy_)
    worker_inspector_proxy_->DispatchMessageFromWorker(session_id, message);
}

ThreadableLoadingContext*
ThreadedMessagingProxyBase::CreateThreadableLoadingContext() const {
  DCHECK(IsParentContextThread());
  return ThreadableLoadingContext::Create(*ToDocument(execution_context_));
}

ExecutionContext* ThreadedMessagingProxyBase::GetExecutionContext() const {
  DCHECK(IsParentContextThread());
  return execution_context_;
}

ParentFrameTaskRunners* ThreadedMessagingProxyBase::GetParentFrameTaskRunners()
    const {
  DCHECK(IsParentContextThread());
  return parent_frame_task_runners_;
}

WorkerInspectorProxy* ThreadedMessagingProxyBase::GetWorkerInspectorProxy()
    const {
  DCHECK(IsParentContextThread());
  return worker_inspector_proxy_;
}

WorkerThread* ThreadedMessagingProxyBase::GetWorkerThread() const {
  DCHECK(IsParentContextThread());
  return worker_thread_.get();
}

WorkerClients* ThreadedMessagingProxyBase::ReleaseWorkerClients() {
  DCHECK(IsParentContextThread());
  DCHECK(worker_clients_);
  return worker_clients_.Release();
}

bool ThreadedMessagingProxyBase::IsParentContextThread() const {
  // TODO(nhiroki): Nested worker is not supported yet, so the parent context
  // thread should be equal to the main thread (http://crbug.com/31666).
  DCHECK(execution_context_->IsDocument());
  return IsMainThread();
}

}  // namespace blink
