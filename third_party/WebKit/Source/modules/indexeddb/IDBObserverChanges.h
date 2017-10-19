// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IDBObserverChanges_h
#define IDBObserverChanges_h

#include "bindings/core/v8/ScriptValue.h"
#include "modules/indexeddb/IDBDatabase.h"
#include "modules/indexeddb/IDBObservation.h"
#include "modules/indexeddb/IDBTransaction.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebVector.h"

namespace blink {

class ScriptState;

class IDBObserverChanges final : public GarbageCollected<IDBObserverChanges>,
                                 public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static IDBObserverChanges* Create(
      IDBDatabase*,
      const WebVector<WebIDBObservation>&,
      const WebVector<int32_t>& observation_indices,
      v8::Isolate*);
  static IDBObserverChanges* Create(
      IDBDatabase*,
      IDBTransaction*,
      const WebVector<WebIDBObservation>&,
      const WebVector<int32_t>& observation_indices,
      v8::Isolate*);

  void Trace(blink::Visitor*);

  // Implement IDL
  IDBTransaction* transaction() const { return transaction_.Get(); }
  IDBDatabase* database() const { return database_.Get(); }
  ScriptValue records(ScriptState*);

 private:
  IDBObserverChanges(IDBDatabase*,
                     IDBTransaction*,
                     const WebVector<WebIDBObservation>&,
                     const WebVector<int32_t>& observation_indices,
                     v8::Isolate*);

  void ExtractChanges(const WebVector<WebIDBObservation>&,
                      const WebVector<int32_t>& observation_indices,
                      v8::Isolate*);

  Member<IDBDatabase> database_;
  Member<IDBTransaction> transaction_;
  // Map object_store_id to IDBObservation list.
  HeapHashMap<int64_t, HeapVector<Member<IDBObservation>>> records_;
};

}  // namespace blink

#endif  // IDBObserverChanges_h
