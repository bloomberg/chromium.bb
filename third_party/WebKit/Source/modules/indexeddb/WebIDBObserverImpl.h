// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebIDBObserverImpl_h
#define WebIDBObserverImpl_h

#include "platform/heap/Persistent.h"
#include "public/platform/modules/indexeddb/WebIDBObserver.h"

namespace blink {

class IDBObserver;

class WebIDBObserverImpl final : public WebIDBObserver {
    USING_FAST_MALLOC(WebIDBObserverImpl);

public:
    static std::unique_ptr<WebIDBObserverImpl> create(IDBObserver*);

    ~WebIDBObserverImpl() override;

    void setId(int32_t);

private:
    enum { kInvalidObserverId = -1 };

    explicit WebIDBObserverImpl(IDBObserver*);

    int32_t m_id;
    Persistent<IDBObserver> m_observer;
};

} // namespace blink

#endif // WebIDBObserverImpl_h
