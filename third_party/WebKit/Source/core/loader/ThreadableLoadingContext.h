// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ThreadableLoadingContext_h
#define ThreadableLoadingContext_h

#include "core/CoreExport.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/frame/UseCounter.h"
#include "platform/heap/GarbageCollected.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class Document;
class ResourceFetcher;
class SecurityOrigin;
class WebTaskRunner;

// An abstract interface for top-level loading context.
// This should be accessed only from the thread where the loading
// context is bound to (e.g. on the main thread).
class CORE_EXPORT ThreadableLoadingContext
    : public GarbageCollected<ThreadableLoadingContext> {
  WTF_MAKE_NONCOPYABLE(ThreadableLoadingContext);

 public:
  static ThreadableLoadingContext* Create(Document&);

  ThreadableLoadingContext() = default;
  virtual ~ThreadableLoadingContext() = default;

  virtual bool IsContextThread() const = 0;

  virtual ResourceFetcher* GetResourceFetcher() = 0;
  virtual SecurityOrigin* GetSecurityOrigin() = 0;
  virtual bool IsSecureContext() const = 0;
  virtual KURL FirstPartyForCookies() const = 0;
  virtual String UserAgent() const = 0;
  virtual RefPtr<WebTaskRunner> GetTaskRunner(TaskType) = 0;
  virtual void RecordUseCount(UseCounter::Feature) = 0;

  // TODO(kinuko): Try getting rid of dependency to Document.
  virtual Document* GetLoadingDocument() { return nullptr; }

  DEFINE_INLINE_VIRTUAL_TRACE() {}
};

}  // namespace blink

#endif  // ThreadableLoadingContext_h
