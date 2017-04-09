// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/ThreadableLoadingContext.h"

#include "core/dom/Document.h"
#include "core/dom/TaskRunnerHelper.h"
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

ThreadableLoadingContext* ThreadableLoadingContext::Create(Document& document) {
  // For now this is the only default implementation.
  return new DocumentThreadableLoadingContext(document);
}

}  // namespace blink
