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

  ExecutionContext* GetExecutionContext() override {
    DCHECK(IsContextThread());
    return document_.Get();
  }

  virtual void Trace(blink::Visitor* visitor) {
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
      : worker_global_scope_(&worker_global_scope) {}

  ~WorkerThreadableLoadingContext() override = default;

  ResourceFetcher* GetResourceFetcher() override {
    DCHECK(IsContextThread());
    return worker_global_scope_->EnsureFetcher();
  }

  ExecutionContext* GetExecutionContext() override {
    DCHECK(IsContextThread());
    return worker_global_scope_.Get();
  }

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(worker_global_scope_);
    ThreadableLoadingContext::Trace(visitor);
  }

 private:
  bool IsContextThread() const {
    DCHECK(worker_global_scope_);
    return worker_global_scope_->IsContextThread();
  }

  Member<WorkerGlobalScope> worker_global_scope_;
};

ThreadableLoadingContext* ThreadableLoadingContext::Create(Document& document) {
  return new DocumentThreadableLoadingContext(document);
}

ThreadableLoadingContext* ThreadableLoadingContext::Create(
    WorkerGlobalScope& worker_global_scope) {
  return new WorkerThreadableLoadingContext(worker_global_scope);
}

BaseFetchContext* ThreadableLoadingContext::GetFetchContext() {
  return static_cast<BaseFetchContext*>(&GetResourceFetcher()->Context());
}

}  // namespace blink
