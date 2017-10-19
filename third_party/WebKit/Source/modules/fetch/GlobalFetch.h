// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GlobalFetch_h
#define GlobalFetch_h

#include "bindings/core/v8/ScriptPromise.h"
#include "modules/ModulesExport.h"
#include "modules/fetch/Request.h"

namespace blink {

class Dictionary;
class LocalDOMWindow;
class ExceptionState;
class ScriptState;
class WorkerGlobalScope;

class GlobalFetch {
  STATIC_ONLY(GlobalFetch);

 public:
  class MODULES_EXPORT ScopedFetcher : public GarbageCollectedMixin {
   public:
    virtual ~ScopedFetcher();

    virtual ScriptPromise Fetch(ScriptState*,
                                const RequestInfo&,
                                const Dictionary&,
                                ExceptionState&) = 0;

    static ScopedFetcher* From(LocalDOMWindow&);
    static ScopedFetcher* From(WorkerGlobalScope&);

    virtual void Trace(blink::Visitor*);
  };

  static ScriptPromise fetch(ScriptState*,
                             LocalDOMWindow&,
                             const RequestInfo&,
                             const Dictionary&,
                             ExceptionState&);
  static ScriptPromise fetch(ScriptState*,
                             WorkerGlobalScope&,
                             const RequestInfo&,
                             const Dictionary&,
                             ExceptionState&);
};

}  // namespace blink

#endif  // GlobalFetch_h
