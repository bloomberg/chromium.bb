// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IDBObserver_h
#define IDBObserver_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/ModulesExport.h"
#include "platform/heap/Handle.h"
#include <set>

namespace blink {

class ExceptionState;
class IDBDatabase;
class IDBObserverCallback;
class IDBObserverInit;
class IDBTransaction;

class MODULES_EXPORT IDBObserver final : public GarbageCollectedFinalized<IDBObserver>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();

public:
    static IDBObserver* create(IDBObserverCallback&, const IDBObserverInit&);

    ~IDBObserver();
    // API methods
    void observe(IDBDatabase*, IDBTransaction*, ExceptionState&);
    void unobserve(IDBDatabase*, ExceptionState&);
    void removeObserver(int32_t id);

    DECLARE_TRACE();

private:
    IDBObserver(IDBObserverCallback&, const IDBObserverInit&);

    Member<IDBObserverCallback> m_callback;
    bool m_transaction;
    bool m_values;
    bool m_noRecords;
    std::set<int32_t> m_observerIds;
};

} // namespace blink

#endif // IDBObserver_h
