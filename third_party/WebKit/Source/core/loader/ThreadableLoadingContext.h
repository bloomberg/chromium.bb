// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ThreadableLoadingContext_h
#define ThreadableLoadingContext_h

#include "base/macros.h"
#include "core/CoreExport.h"
#include "platform/heap/Heap.h"
#include "platform/wtf/Forward.h"

namespace blink {

class BaseFetchContext;
class ExecutionContext;
class ResourceFetcher;

// A convenient holder for various contexts associated with the loading
// activity. This should be accessed only from the thread where the loading
// context is bound to (e.g. on the main thread).
class CORE_EXPORT ThreadableLoadingContext
    : public GarbageCollected<ThreadableLoadingContext> {
 public:
  static ThreadableLoadingContext* Create(ExecutionContext&);

  ThreadableLoadingContext() = default;
  virtual ~ThreadableLoadingContext() = default;

  virtual ResourceFetcher* GetResourceFetcher() = 0;
  virtual ExecutionContext* GetExecutionContext() = 0;
  BaseFetchContext* GetFetchContext();

  virtual void Trace(blink::Visitor* visitor) {}

  DISALLOW_COPY_AND_ASSIGN(ThreadableLoadingContext);
};

}  // namespace blink

#endif  // ThreadableLoadingContext_h
