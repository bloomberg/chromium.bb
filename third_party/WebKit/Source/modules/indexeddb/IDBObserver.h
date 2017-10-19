// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IDBObserver_h
#define IDBObserver_h

#include "modules/ModulesExport.h"
#include "platform/bindings/ScriptState.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/TraceWrapperMember.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebVector.h"
#include "public/platform/modules/indexeddb/WebIDBTypes.h"

namespace blink {

class ExceptionState;
class IDBDatabase;
class IDBObserverInit;
class IDBTransaction;
class V8IDBObserverCallback;

class MODULES_EXPORT IDBObserver final : public GarbageCollected<IDBObserver>,
                                         public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static IDBObserver* Create(V8IDBObserverCallback*);

  V8IDBObserverCallback* Callback() { return callback_; }

  // Implement the IDBObserver IDL.
  void observe(IDBDatabase*,
               IDBTransaction*,
               const IDBObserverInit&,
               ExceptionState&);
  void unobserve(IDBDatabase*, ExceptionState&);

  void Trace(blink::Visitor*);
  void TraceWrappers(const ScriptWrappableVisitor*) const;

 private:
  explicit IDBObserver(V8IDBObserverCallback*);

  TraceWrapperMember<V8IDBObserverCallback> callback_;
  HeapHashMap<int32_t, WeakMember<IDBDatabase>> observer_ids_;
};

}  // namespace blink

#endif  // IDBObserver_h
