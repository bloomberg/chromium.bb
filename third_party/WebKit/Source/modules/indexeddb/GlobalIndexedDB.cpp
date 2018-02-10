// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/indexeddb/GlobalIndexedDB.h"

#include "core/frame/LocalDOMWindow.h"
#include "core/workers/WorkerGlobalScope.h"
#include "modules/indexeddb/IDBFactory.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

namespace {

template <typename T>
class GlobalIndexedDBImpl final
    : public GarbageCollected<GlobalIndexedDBImpl<T>>,
      public Supplement<T> {
  USING_GARBAGE_COLLECTED_MIXIN(GlobalIndexedDBImpl);

 public:
  static const char kSupplementName[];

  static GlobalIndexedDBImpl& From(T& supplementable) {
    GlobalIndexedDBImpl* supplement =
        Supplement<T>::template From<GlobalIndexedDBImpl>(supplementable);
    if (!supplement) {
      supplement = new GlobalIndexedDBImpl;
      Supplement<T>::ProvideTo(supplementable, supplement);
    }
    return *supplement;
  }

  IDBFactory* IdbFactory(T& fetching_scope) {
    if (!idb_factory_)
      idb_factory_ = IDBFactory::Create();
    return idb_factory_;
  }

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(idb_factory_);
    Supplement<T>::Trace(visitor);
  }

 private:
  GlobalIndexedDBImpl() = default;

  Member<IDBFactory> idb_factory_;
};

// static
template <typename T>
const char GlobalIndexedDBImpl<T>::kSupplementName[] = "GlobalIndexedDBImpl";

}  // namespace

IDBFactory* GlobalIndexedDB::indexedDB(LocalDOMWindow& window) {
  return GlobalIndexedDBImpl<LocalDOMWindow>::From(window).IdbFactory(window);
}

IDBFactory* GlobalIndexedDB::indexedDB(WorkerGlobalScope& worker) {
  return GlobalIndexedDBImpl<WorkerGlobalScope>::From(worker).IdbFactory(
      worker);
}

}  // namespace blink
