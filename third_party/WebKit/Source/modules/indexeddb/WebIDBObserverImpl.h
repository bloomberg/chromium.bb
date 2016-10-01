// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebIDBObserverImpl_h
#define WebIDBObserverImpl_h

#include "modules/indexeddb/IDBObserver.h"
#include "platform/heap/Persistent.h"
#include "public/platform/modules/indexeddb/WebIDBObserver.h"

namespace blink {

class IDBObserver;
struct WebIDBObservation;

class WebIDBObserverImpl final : public WebIDBObserver {
  USING_FAST_MALLOC(WebIDBObserverImpl);

 public:
  static std::unique_ptr<WebIDBObserverImpl> create(IDBObserver*);

  ~WebIDBObserverImpl() override;

  void setId(int32_t);

  bool transaction() const { return m_observer->transaction(); }
  bool noRecords() const { return m_observer->noRecords(); }
  bool values() const { return m_observer->values(); }
  const std::bitset<WebIDBOperationTypeCount>& operationTypes() const {
    return m_observer->operationTypes();
  }
  void onChange(const WebVector<WebIDBObservation>&,
                const WebVector<int32_t>& observationIndex);

 private:
  enum { kInvalidObserverId = -1 };

  explicit WebIDBObserverImpl(IDBObserver*);

  int32_t m_id;
  Persistent<IDBObserver> m_observer;
};

}  // namespace blink

#endif  // WebIDBObserverImpl_h
