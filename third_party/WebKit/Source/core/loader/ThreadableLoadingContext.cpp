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
      : m_document(&document) {}

  ~DocumentThreadableLoadingContext() override = default;

  bool isContextThread() const override {
    return m_document->isContextThread();
  }

  ResourceFetcher* getResourceFetcher() override {
    DCHECK(isContextThread());
    return m_document->fetcher();
  }

  SecurityOrigin* getSecurityOrigin() override {
    DCHECK(isContextThread());
    return m_document->getSecurityOrigin();
  }

  bool isSecureContext() const override {
    DCHECK(isContextThread());
    return m_document->isSecureContext();
  }

  KURL firstPartyForCookies() const override {
    DCHECK(isContextThread());
    return m_document->firstPartyForCookies();
  }

  String userAgent() const override {
    DCHECK(isContextThread());
    return m_document->userAgent();
  }

  Document* getLoadingDocument() override {
    DCHECK(isContextThread());
    return m_document.get();
  }

  RefPtr<WebTaskRunner> getTaskRunner(TaskType type) override {
    return TaskRunnerHelper::get(type, m_document.get());
  }

  void recordUseCount(UseCounter::Feature feature) override {
    UseCounter::count(m_document.get(), feature);
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(m_document);
    ThreadableLoadingContext::trace(visitor);
  }

 private:
  Member<Document> m_document;
};

ThreadableLoadingContext* ThreadableLoadingContext::create(Document& document) {
  // For now this is the only default implementation.
  return new DocumentThreadableLoadingContext(document);
}

}  // namespace blink
