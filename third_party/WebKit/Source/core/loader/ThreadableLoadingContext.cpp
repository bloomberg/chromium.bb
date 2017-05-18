// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/ThreadableLoadingContext.h"

#include "core/dom/Document.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/loader/WorkerFetchContext.h"
#include "core/workers/WorkerGlobalScope.h"
#include "platform/loader/fetch/ResourceFetcher.h"

namespace blink {

class DocumentThreadableLoadingContext final : public ThreadableLoadingContext {
 public:
  explicit DocumentThreadableLoadingContext(Document& document)
      : document_(&document) {}

  ~DocumentThreadableLoadingContext() override = default;

  bool IsContextThread() const override { return document_->IsContextThread(); }

  ResourceFetcher* GetResourceFetcher() override {
    DCHECK(IsContextThread());
    return document_->Fetcher();
  }

  SecurityOrigin* GetSecurityOrigin() override {
    DCHECK(IsContextThread());
    return document_->GetSecurityOrigin();
  }

  bool IsSecureContext() const override {
    DCHECK(IsContextThread());
    return document_->IsSecureContext();
  }

  KURL FirstPartyForCookies() const override {
    DCHECK(IsContextThread());
    return document_->FirstPartyForCookies();
  }

  String UserAgent() const override {
    DCHECK(IsContextThread());
    return document_->UserAgent();
  }

  Document* GetLoadingDocument() override {
    DCHECK(IsContextThread());
    return document_.Get();
  }

  RefPtr<WebTaskRunner> GetTaskRunner(TaskType type) override {
    return TaskRunnerHelper::Get(type, document_.Get());
  }

  void RecordUseCount(UseCounter::Feature feature) override {
    UseCounter::Count(document_.Get(), feature);
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(document_);
    ThreadableLoadingContext::Trace(visitor);
  }

 private:
  Member<Document> document_;
};

class WorkerThreadableLoadingContext : public ThreadableLoadingContext {
 public:
  explicit WorkerThreadableLoadingContext(
      WorkerGlobalScope& worker_global_scope)
      : worker_global_scope_(&worker_global_scope),
        fetch_context_(worker_global_scope.GetFetchContext()) {}

  ~WorkerThreadableLoadingContext() override = default;

  bool IsContextThread() const override {
    DCHECK(fetch_context_);
    DCHECK(worker_global_scope_);
    return worker_global_scope_->IsContextThread();
  }

  ResourceFetcher* GetResourceFetcher() override {
    DCHECK(IsContextThread());
    return fetch_context_->GetResourceFetcher();
  }

  SecurityOrigin* GetSecurityOrigin() override {
    DCHECK(IsContextThread());
    return worker_global_scope_->GetSecurityOrigin();
  }

  bool IsSecureContext() const override {
    DCHECK(IsContextThread());
    String error_message;
    return worker_global_scope_->IsSecureContext(error_message);
  }

  KURL FirstPartyForCookies() const override {
    DCHECK(IsContextThread());
    return fetch_context_->FirstPartyForCookies();
  }

  String UserAgent() const override {
    DCHECK(IsContextThread());
    return worker_global_scope_->UserAgent();
  }

  Document* GetLoadingDocument() override { return nullptr; }

  RefPtr<WebTaskRunner> GetTaskRunner(TaskType type) override {
    return fetch_context_->LoadingTaskRunner();
  }

  void RecordUseCount(UseCounter::Feature feature) override {
    UseCounter::Count(worker_global_scope_, feature);
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(fetch_context_);
    visitor->Trace(worker_global_scope_);
    ThreadableLoadingContext::Trace(visitor);
  }

 private:
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
