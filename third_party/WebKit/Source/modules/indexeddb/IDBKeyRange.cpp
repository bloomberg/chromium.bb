/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "modules/indexeddb/IDBKeyRange.h"

#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/IDBBindingUtilities.h"
#include "core/dom/ExceptionCode.h"
#include "modules/indexeddb/IDBDatabase.h"

namespace WebCore {

PassRefPtr<IDBKeyRange> IDBKeyRange::fromScriptValue(ExecutionContext* context, const ScriptValue& value, ExceptionState& exceptionState)
{
    if (value.isUndefined() || value.isNull())
        return nullptr;

    RefPtr<IDBKeyRange> range = scriptValueToIDBKeyRange(toIsolate(context), value);
    if (range)
        return range.release();

    RefPtr<IDBKey> key = scriptValueToIDBKey(toIsolate(context), value);
    if (!key || !key->isValid()) {
        exceptionState.throwDOMException(DataError, IDBDatabase::notValidKeyErrorMessage);
        return nullptr;
    }

    return adoptRef(new IDBKeyRange(key, key, LowerBoundClosed, UpperBoundClosed));
}

IDBKeyRange::IDBKeyRange(PassRefPtr<IDBKey> lower, PassRefPtr<IDBKey> upper, LowerBoundType lowerType, UpperBoundType upperType)
    : m_lower(lower)
    , m_upper(upper)
    , m_lowerType(lowerType)
    , m_upperType(upperType)
{
    ScriptWrappable::init(this);
}

ScriptValue IDBKeyRange::lowerValue(NewScriptState* scriptState) const
{
    return idbKeyToScriptValue(scriptState, m_lower);
}

ScriptValue IDBKeyRange::upperValue(NewScriptState* scriptState) const
{
    return idbKeyToScriptValue(scriptState, m_upper);
}

PassRefPtr<IDBKeyRange> IDBKeyRange::only(PassRefPtr<IDBKey> prpKey, ExceptionState& exceptionState)
{
    RefPtr<IDBKey> key = prpKey;
    if (!key || !key->isValid()) {
        exceptionState.throwDOMException(DataError, IDBDatabase::notValidKeyErrorMessage);
        return nullptr;
    }

    return IDBKeyRange::create(key, key, LowerBoundClosed, UpperBoundClosed);
}

PassRefPtr<IDBKeyRange> IDBKeyRange::only(ExecutionContext* context, const ScriptValue& keyValue, ExceptionState& exceptionState)
{
    RefPtr<IDBKey> key = scriptValueToIDBKey(toIsolate(context), keyValue);
    if (!key || !key->isValid()) {
        exceptionState.throwDOMException(DataError, IDBDatabase::notValidKeyErrorMessage);
        return nullptr;
    }

    return IDBKeyRange::create(key, key, LowerBoundClosed, UpperBoundClosed);
}

PassRefPtr<IDBKeyRange> IDBKeyRange::lowerBound(ExecutionContext* context, const ScriptValue& boundValue, bool open, ExceptionState& exceptionState)
{
    RefPtr<IDBKey> bound = scriptValueToIDBKey(toIsolate(context), boundValue);
    if (!bound || !bound->isValid()) {
        exceptionState.throwDOMException(DataError, IDBDatabase::notValidKeyErrorMessage);
        return nullptr;
    }

    return IDBKeyRange::create(bound, nullptr, open ? LowerBoundOpen : LowerBoundClosed, UpperBoundOpen);
}

PassRefPtr<IDBKeyRange> IDBKeyRange::upperBound(ExecutionContext* context, const ScriptValue& boundValue, bool open, ExceptionState& exceptionState)
{
    RefPtr<IDBKey> bound = scriptValueToIDBKey(toIsolate(context), boundValue);
    if (!bound || !bound->isValid()) {
        exceptionState.throwDOMException(DataError, IDBDatabase::notValidKeyErrorMessage);
        return nullptr;
    }

    return IDBKeyRange::create(nullptr, bound, LowerBoundOpen, open ? UpperBoundOpen : UpperBoundClosed);
}

PassRefPtr<IDBKeyRange> IDBKeyRange::bound(ExecutionContext* context, const ScriptValue& lowerValue, const ScriptValue& upperValue, bool lowerOpen, bool upperOpen, ExceptionState& exceptionState)
{
    RefPtr<IDBKey> lower = scriptValueToIDBKey(toIsolate(context), lowerValue);
    RefPtr<IDBKey> upper = scriptValueToIDBKey(toIsolate(context), upperValue);

    if (!lower || !lower->isValid() || !upper || !upper->isValid()) {
        exceptionState.throwDOMException(DataError, IDBDatabase::notValidKeyErrorMessage);
        return nullptr;
    }
    if (upper->isLessThan(lower.get())) {
        exceptionState.throwDOMException(DataError, "The lower key is greater than the upper key.");
        return nullptr;
    }
    if (upper->isEqual(lower.get()) && (lowerOpen || upperOpen)) {
        exceptionState.throwDOMException(DataError, "The lower key and upper key are equal and one of the bounds is open.");
        return nullptr;
    }

    return IDBKeyRange::create(lower, upper, lowerOpen ? LowerBoundOpen : LowerBoundClosed, upperOpen ? UpperBoundOpen : UpperBoundClosed);
}

} // namespace WebCore
