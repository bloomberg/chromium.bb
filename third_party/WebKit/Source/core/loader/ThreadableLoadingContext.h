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
#include "wtf/Forward.h"
#include "wtf/Noncopyable.h"

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
  static ThreadableLoadingContext* create(Document&);

  ThreadableLoadingContext() = default;
  virtual ~ThreadableLoadingContext() = default;

  virtual bool isContextThread() const = 0;

  virtual ResourceFetcher* getResourceFetcher() = 0;
  virtual SecurityOrigin* getSecurityOrigin() = 0;
  virtual bool isSecureContext() const = 0;
  virtual KURL firstPartyForCookies() const = 0;
  virtual String userAgent() const = 0;
  virtual RefPtr<WebTaskRunner> getTaskRunner(TaskType) = 0;
  virtual void recordUseCount(UseCounter::Feature) = 0;

  // TODO(kinuko): Try getting rid of dependency to Document.
  virtual Document* getLoadingDocument() { return nullptr; }

  DEFINE_INLINE_VIRTUAL_TRACE() {}
};

}  // namespace blink

#endif  // ThreadableLoadingContext_h
