// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/ThreadedMessagingProxyBase.h"

#include "bindings/core/v8/SourceLocation.h"
#include "core/dom/Document.h"
#include "core/frame/Deprecation.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/ThreadableLoadingContext.h"
#include "core/workers/WorkerInspectorProxy.h"
#include "core/workers/WorkerThreadStartupData.h"
#include "platform/wtf/CurrentTime.h"

namespace blink {

namespace {

static int g_live_messaging_proxy_count = 0;

}  // namespace

ThreadedMessagingProxyBase::ThreadedMessagingProxyBase(
    ExecutionContext* execution_context)
    : execution_context_(execution_context),
      worker_inspector_proxy_(WorkerInspectorProxy::Create()),
      parent_frame_task_runners_(ParentFrameTaskRunners::Create(
          ToDocument(execution_context_.Get())->GetFrame())),
      may_be_destroyed_(false),
      asked_to_terminate_(false) {
  DCHECK(IsParentContextThread());
  g_live_messaging_proxy_count++;
}

ThreadedMessagingProxyBase::~ThreadedMessagingProxyBase() {
  DCHECK(IsParentContextThread());
  if (loader_proxy_)
    loader_proxy_->DetachProvider(this);
  g_live_messaging_proxy_count--;
}

int ThreadedMessagingProxyBase::ProxyCount() {
  DCHECK(IsMainThread());
  return g_live_messaging_proxy_count;
}

void ThreadedMessagingProxyBase::SetWorkerThreadForTest(
    std::unique_ptr<WorkerThread> worker_thread) {
  worker_thread_ = std::move(worker_thread);
}

void ThreadedMessagingProxyBase::InitializeWorkerThread(
    std::unique_ptr<WorkerThreadStartupData> startup_data) {
  DCHECK(IsParentContextThread());

  Document* document = ToDocument(GetExecutionContext());
  double origin_time =
      document->Loader()
          ? document->Loader()->GetTiming().ReferenceMonotonicTime()
          : MonotonicallyIncreasingTime();

  loader_proxy_ = WorkerLoaderProxy::Create(this);
  worker_thread_ = CreateWorkerThread(origin_time);
  worker_thread_->Start(std::move(startup_data), GetParentFrameTaskRunners());
  WorkerThreadCreated();
}

void ThreadedMessagingProxyBase::PostTaskToWorkerGlobalScope(
    const WebTraceLocation& location,
    std::unique_ptr<WTF::CrossThreadClosure> task) {
  if (asked_to_terminate_)
    return;

  DCHECK(worker_thread_);
  worker_thread_->PostTask(location, std::move(task));
}

void ThreadedMessagingProxyBase::PostTaskToLoader(
    const WebTraceLocation& location,
    std::unique_ptr<WTF::CrossThreadClosure> task) {
  parent_frame_task_runners_->Get(TaskType::kNetworking)
      ->PostTask(BLINK_FROM_HERE, std::move(task));
}

ThreadableLoadingContext*
ThreadedMessagingProxyBase::GetThreadableLoadingContext() {
  DCHECK(IsParentContextThread());
  if (!loading_context_) {
    loading_context_ =
        ThreadableLoadingContext::Create(*ToDocument(execution_context_));
  }
  return loading_context_;
}

void ThreadedMessagingProxyBase::CountFeature(UseCounter::Feature feature) {
  DCHECK(IsParentContextThread());
  UseCounter::Count(execution_context_, feature);
}

void ThreadedMessagingProxyBase::CountDeprecation(UseCounter::Feature feature) {
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

  GetParentFrameTaskRunners()
      ->Get(TaskType::kUnspecedTimer)
      ->PostTask(
          BLINK_FROM_HERE,
          WTF::Bind(&ThreadedMessagingProxyBase::ParentObjectDestroyedInternal,
                    WTF::Unretained(this)));
}

void ThreadedMessagingProxyBase::ParentObjectDestroyedInternal() {
  DCHECK(IsParentContextThread());
  may_be_destroyed_ = true;
  if (worker_thread_)
    TerminateGlobalScope();
  else
    WorkerThreadTerminated();
}

void ThreadedMessagingProxyBase::WorkerThreadTerminated() {
  DCHECK(IsParentContextThread());

  // This method is always the last to be performed, so the proxy is not
  // needed for communication in either side any more. However, the Worker
  // object may still exist, and it assumes that the proxy exists, too.
  asked_to_terminate_ = true;
  worker_thread_ = nullptr;
  worker_inspector_proxy_->WorkerThreadTerminated();
  if (may_be_destroyed_)
    delete this;
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
    const String& message) {
  DCHECK(IsParentContextThread());
  if (worker_inspector_proxy_)
    worker_inspector_proxy_->DispatchMessageFromWorker(message);
}

bool ThreadedMessagingProxyBase::IsParentContextThread() const {
  // TODO(nhiroki): Nested worker is not supported yet, so the parent context
  // thread should be equal to the main thread (http://crbug.com/31666).
  DCHECK(execution_context_->IsDocument());
  return IsMainThread();
}

}  // namespace blink
