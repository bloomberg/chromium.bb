// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/ThreadableLoadingContext.h"

#include "core/dom/Document.h"
#include "core/loader/WorkerFetchContext.h"
#include "core/workers/WorkerGlobalScope.h"
#include "platform/loader/fetch/ResourceFetcher.h"

namespace blink {

class DocumentThreadableLoadingContext final : public ThreadableLoadingContext {
 public:
  explicit DocumentThreadableLoadingContext(Document& document)
      : document_(&document) {}

  ~DocumentThreadableLoadingContext() override = default;

  ResourceFetcher* GetResourceFetcher() override {
    DCHECK(IsContextThread());
    return document_->Fetcher();
  }

  BaseFetchContext* GetFetchContext() override {
    DCHECK(IsContextThread());
    return static_cast<BaseFetchContext*>(&document_->Fetcher()->Context());
  }

  ExecutionContext* GetExecutionContext() override {
    DCHECK(IsContextThread());
    return document_.Get();
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(document_);
    ThreadableLoadingContext::Trace(visitor);
  }

 private:
  bool IsContextThread() const { return document_->IsContextThread(); }

  Member<Document> document_;
};

class WorkerThreadableLoadingContext : public ThreadableLoadingContext {
 public:
  explicit WorkerThreadableLoadingContext(
      WorkerGlobalScope& worker_global_scope)
      : worker_global_scope_(&worker_global_scope),
        fetch_context_(worker_global_scope.GetFetchContext()) {}

  ~WorkerThreadableLoadingContext() override = default;

  ResourceFetcher* GetResourceFetcher() override {
    DCHECK(IsContextThread());
    return fetch_context_->GetResourceFetcher();
  }

  BaseFetchContext* GetFetchContext() override {
    DCHECK(IsContextThread());
    return fetch_context_.Get();
  }

  ExecutionContext* GetExecutionContext() override {
    return worker_global_scope_.Get();
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(fetch_context_);
    visitor->Trace(worker_global_scope_);
    ThreadableLoadingContext::Trace(visitor);
  }

 private:
  bool IsContextThread() const {
    DCHECK(fetch_context_);
    DCHECK(worker_global_scope_);
    return worker_global_scope_->IsContextThread();
  }

  Member<WorkerGlobalScope> worker_global_scope_;
  Member<WorkerFetchContext> fetch_context_;
};

ThreadableLoadingContext* ThreadableLoadingContext::Create(Document& document) {
  return new DocumentThreadableLoadingContext(document);
}

ThreadableLoadingContext* ThreadableLoadingContext::Create(
    WorkerGlobalScope& worker_global_scope) {
  return new WorkerThreadableLoadingContext(worker_global_scope);
}

}  // namespace blink
