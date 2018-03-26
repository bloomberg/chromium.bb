// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/ThreadedMessagingProxyBase.h"

#include "base/synchronization/waitable_event.h"
#include "bindings/core/v8/SourceLocation.h"
#include "core/dom/Document.h"
#include "core/frame/Deprecation.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/ThreadableLoadingContext.h"
#include "core/loader/WorkerFetchContext.h"
#include "core/workers/GlobalScopeCreationParams.h"
#include "core/workers/WorkerInspectorProxy.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/wtf/Time.h"
#include "public/platform/TaskType.h"
#include "public/platform/WebWorkerFetchContext.h"
#include "public/web/WebFrameClient.h"

namespace blink {

namespace {

static int g_live_messaging_proxy_count = 0;

}  // namespace

ThreadedMessagingProxyBase::ThreadedMessagingProxyBase(
    ExecutionContext* execution_context)
    : execution_context_(execution_context),
      worker_inspector_proxy_(WorkerInspectorProxy::Create()),
      parent_frame_task_runners_(ParentFrameTaskRunners::Create(
          *ToDocument(execution_context_.Get())->GetFrame())),
      keep_alive_(this) {
  DCHECK(IsParentContextThread());
  g_live_messaging_proxy_count++;
}

ThreadedMessagingProxyBase::~ThreadedMessagingProxyBase() {
  g_live_messaging_proxy_count--;
}

int ThreadedMessagingProxyBase::ProxyCount() {
  DCHECK(IsMainThread());
  return g_live_messaging_proxy_count;
}

void ThreadedMessagingProxyBase::Trace(blink::Visitor* visitor) {
  visitor->Trace(execution_context_);
  visitor->Trace(worker_inspector_proxy_);
}

void ThreadedMessagingProxyBase::InitializeWorkerThread(
    std::unique_ptr<GlobalScopeCreationParams> global_scope_creation_params,
    const WTF::Optional<WorkerBackingThreadStartupData>& thread_startup_data) {
  DCHECK(IsParentContextThread());

  Document* document = ToDocument(GetExecutionContext());
  KURL script_url = global_scope_creation_params->script_url.Copy();

  WebLocalFrameImpl* web_frame =
      WebLocalFrameImpl::FromFrame(document->GetFrame());
  // |web_frame| is null in some unit tests.
  if (web_frame) {
    std::unique_ptr<WebWorkerFetchContext> web_worker_fetch_context =
        web_frame->Client()->CreateWorkerFetchContext();
    DCHECK(web_worker_fetch_context);
    terminate_sync_load_event_ =
        web_worker_fetch_context->GetTerminateSyncLoadEvent();
    web_worker_fetch_context->SetApplicationCacheHostID(
        document->Fetcher()->Context().ApplicationCacheHostID());
    web_worker_fetch_context->SetIsOnSubframe(
        document->GetFrame() != document->GetFrame()->Tree().Top());
    ProvideWorkerFetchContextToWorker(
        global_scope_creation_params->worker_clients,
        std::move(web_worker_fetch_context));
  }

  worker_thread_ = CreateWorkerThread();
  worker_thread_->Start(
      std::move(global_scope_creation_params), thread_startup_data,
      GetWorkerInspectorProxy()->ShouldPauseOnWorkerStart(document),
      GetParentFrameTaskRunners());
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
  execution_context_->AddConsoleMessage(ConsoleMessage::CreateFromWorker(
      level, message, std::move(location), worker_thread_.get()));
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

  if (terminate_sync_load_event_) {
    terminate_sync_load_event_->Signal();
    terminate_sync_load_event_ = nullptr;
  }

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

bool ThreadedMessagingProxyBase::IsParentContextThread() const {
  // TODO(nhiroki): Nested worker is not supported yet, so the parent context
  // thread should be equal to the main thread (http://crbug.com/31666).
  DCHECK(execution_context_->IsDocument());
  return IsMainThread();
}

}  // namespace blink
