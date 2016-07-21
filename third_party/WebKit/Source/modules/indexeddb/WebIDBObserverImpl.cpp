// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/indexeddb/WebIDBObserverImpl.h"

#include "wtf/PtrUtil.h"

namespace blink {

// static
std::unique_ptr<WebIDBObserverImpl> WebIDBObserverImpl::create(IDBObserver* observer)
{
    return wrapUnique(new WebIDBObserverImpl(observer));
}

WebIDBObserverImpl::WebIDBObserverImpl(IDBObserver* observer)
    : m_id(kInvalidObserverId)
    , m_observer(observer)
{
}

// Remove observe call id from IDBObserver.
WebIDBObserverImpl::~WebIDBObserverImpl()
{
    if (m_id != kInvalidObserverId)
        m_observer->removeObserver(m_id);
}

void WebIDBObserverImpl::setId(int32_t id)
{
    DCHECK_EQ(kInvalidObserverId, m_id);
    m_id = id;
}

void WebIDBObserverImpl::onChange(const WebVector<WebIDBObservation>& observations, const WebVector<int32_t>& observationIndex)
{
    m_observer->onChange(m_id, observations, std::move(observationIndex));
}

} // namespace blink
