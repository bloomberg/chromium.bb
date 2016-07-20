// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/indexeddb/IDBObservation.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ToV8.h"
#include "bindings/modules/v8/ToV8ForModules.h"
#include "bindings/modules/v8/V8BindingForModules.h"
#include "modules/IndexedDBNames.h"
#include "modules/indexeddb/IDBAny.h"
#include "modules/indexeddb/IDBKey.h"
#include "modules/indexeddb/IDBValue.h"

namespace blink {

IDBObservation::~IDBObservation() {}

ScriptValue IDBObservation::key(ScriptState* scriptState)
{
    return ScriptValue::from(scriptState, m_key);
}

ScriptValue IDBObservation::value(ScriptState* scriptState)
{
    IDBAny* value;
    if (!m_value) {
        value = IDBAny::createUndefined();
    } else {
        value = IDBAny::create(m_value);
    }
    ScriptValue scriptValue = ScriptValue::from(scriptState, value);
    return scriptValue;
}

WebIDBOperationType IDBObservation::stringToOperationType(const String& type)
{
    if (type == IndexedDBNames::add)
        return WebIDBAdd;
    if (type == IndexedDBNames::put)
        return WebIDBPut;
    if (type == IndexedDBNames::kDelete)
        return WebIDBDelete;
    if (type == IndexedDBNames::clear)
        return WebIDBClear;

    NOTREACHED();
    return WebIDBAdd;
}

const String& IDBObservation::type() const
{
    switch (m_operationType) {
    case WebIDBAdd:
        return IndexedDBNames::add;

    case WebIDBPut:
        return IndexedDBNames::put;

    case WebIDBDelete:
        return IndexedDBNames::kDelete;

    case WebIDBClear:
        return IndexedDBNames::clear;

    default:
        NOTREACHED();
        return IndexedDBNames::add;
    }
}

IDBObservation* IDBObservation::create(IDBKey* key, PassRefPtr<IDBValue> value, WebIDBOperationType type)
{
    return new IDBObservation(key, value, type);
}

IDBObservation::IDBObservation(IDBKey* key, PassRefPtr<IDBValue> value, WebIDBOperationType type)
    : m_key(key)
    , m_value(value)
    , m_operationType(type)
{
}

DEFINE_TRACE(IDBObservation)
{
    visitor->trace(m_key);
}

} // namespace blink
