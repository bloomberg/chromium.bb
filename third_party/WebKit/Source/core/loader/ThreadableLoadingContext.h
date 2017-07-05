// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ThreadableLoadingContext_h
#define ThreadableLoadingContext_h

#include "core/CoreExport.h"
#include "platform/heap/Heap.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class BaseFetchContext;
class Document;
class ExecutionContext;
class ResourceFetcher;
class WorkerGlobalScope;

// A convenient holder for various contexts associated with the loading
// activity. This should be accessed only from the thread where the loading
// context is bound to (e.g. on the main thread).
class CORE_EXPORT ThreadableLoadingContext
    : public GarbageCollected<ThreadableLoadingContext> {
  WTF_MAKE_NONCOPYABLE(ThreadableLoadingContext);

 public:
  static ThreadableLoadingContext* Create(Document&);
  static ThreadableLoadingContext* Create(WorkerGlobalScope&);

  ThreadableLoadingContext() = default;
  virtual ~ThreadableLoadingContext() = default;

  virtual ResourceFetcher* GetResourceFetcher() = 0;
  virtual BaseFetchContext* GetFetchContext() = 0;
  virtual ExecutionContext* GetExecutionContext() = 0;

  DEFINE_INLINE_VIRTUAL_TRACE() {}
};

}  // namespace blink

#endif  // ThreadableLoadingContext_h
