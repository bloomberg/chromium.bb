// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IDBObserver_h
#define IDBObserver_h

#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/ModulesExport.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebVector.h"
#include "public/platform/modules/indexeddb/WebIDBTypes.h"
#include <bitset>

namespace blink {

class ExceptionState;
class IDBDatabase;
class IDBObserverCallback;
class IDBObserverInit;
class IDBTransaction;
struct WebIDBObservation;

class MODULES_EXPORT IDBObserver final
    : public GarbageCollectedFinalized<IDBObserver>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static IDBObserver* create(IDBObserverCallback*, const IDBObserverInit&);

  void removeObserver(int32_t id);
  void onChange(int32_t id,
                const WebVector<WebIDBObservation>&,
                const WebVector<int32_t>& observationIndex);

  bool transaction() const { return m_transaction; }
  bool noRecords() const { return m_noRecords; }
  bool values() const { return m_values; }
  const std::bitset<WebIDBOperationTypeCount>& operationTypes() const {
    return m_operationTypes;
  }

  // Implement the IDBObserver IDL.
  void observe(IDBDatabase*, IDBTransaction*, ExceptionState&);
  void unobserve(IDBDatabase*, ExceptionState&);

  DECLARE_TRACE();

 private:
  IDBObserver(IDBObserverCallback*, const IDBObserverInit&);

  Member<IDBObserverCallback> m_callback;
  bool m_transaction;
  bool m_values;
  bool m_noRecords;
  // Operation type bits are set corresponding to WebIDBOperationType.
  std::bitset<WebIDBOperationTypeCount> m_operationTypes;
  HeapHashMap<int32_t, WeakMember<IDBDatabase>> m_observerIds;
};

}  // namespace blink

#endif  // IDBObserver_h
