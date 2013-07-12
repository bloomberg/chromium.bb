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

#include "bindings/v8/DOMRequestState.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/IDBBindingUtilities.h"
#include "core/dom/ExceptionCode.h"
#include "modules/indexeddb/IDBDatabase.h"
#include "modules/indexeddb/IDBKey.h"

namespace WebCore {

PassRefPtr<IDBKeyRange> IDBKeyRange::create(PassRefPtr<IDBKey> prpKey)
{
    RefPtr<IDBKey> key = prpKey;
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

ScriptValue IDBKeyRange::lowerValue(ScriptExecutionContext* context) const
{
    DOMRequestState requestState(context);
    return idbKeyToScriptValue(&requestState, m_lower);
}

ScriptValue IDBKeyRange::upperValue(ScriptExecutionContext* context) const
{
    DOMRequestState requestState(context);
    return idbKeyToScriptValue(&requestState, m_upper);
}

PassRefPtr<IDBKeyRange> IDBKeyRange::only(PassRefPtr<IDBKey> prpKey, ExceptionState& es)
{
    RefPtr<IDBKey> key = prpKey;
    if (!key || !key->isValid()) {
        es.throwDOMException(DataError, IDBDatabase::notValidKeyErrorMessage);
        return 0;
    }

    return IDBKeyRange::create(key, key, LowerBoundClosed, UpperBoundClosed);
}

PassRefPtr<IDBKeyRange> IDBKeyRange::only(ScriptExecutionContext* context, const ScriptValue& keyValue, ExceptionState& es)
{
    DOMRequestState requestState(context);
    RefPtr<IDBKey> key = scriptValueToIDBKey(&requestState, keyValue);
    if (!key || !key->isValid()) {
        es.throwDOMException(DataError, IDBDatabase::notValidKeyErrorMessage);
        return 0;
    }

    return IDBKeyRange::create(key, key, LowerBoundClosed, UpperBoundClosed);
}

PassRefPtr<IDBKeyRange> IDBKeyRange::lowerBound(ScriptExecutionContext* context, const ScriptValue& boundValue, bool open, ExceptionState& es)
{
    DOMRequestState requestState(context);
    RefPtr<IDBKey> bound = scriptValueToIDBKey(&requestState, boundValue);
    if (!bound || !bound->isValid()) {
        es.throwDOMException(DataError, IDBDatabase::notValidKeyErrorMessage);
        return 0;
    }

    return IDBKeyRange::create(bound, 0, open ? LowerBoundOpen : LowerBoundClosed, UpperBoundOpen);
}

PassRefPtr<IDBKeyRange> IDBKeyRange::upperBound(ScriptExecutionContext* context, const ScriptValue& boundValue, bool open, ExceptionState& es)
{
    DOMRequestState requestState(context);
    RefPtr<IDBKey> bound = scriptValueToIDBKey(&requestState, boundValue);
    if (!bound || !bound->isValid()) {
        es.throwDOMException(DataError, IDBDatabase::notValidKeyErrorMessage);
        return 0;
    }

    return IDBKeyRange::create(0, bound, LowerBoundOpen, open ? UpperBoundOpen : UpperBoundClosed);
}

PassRefPtr<IDBKeyRange> IDBKeyRange::bound(ScriptExecutionContext* context, const ScriptValue& lowerValue, const ScriptValue& upperValue, bool lowerOpen, bool upperOpen, ExceptionState& es)
{
    DOMRequestState requestState(context);
    RefPtr<IDBKey> lower = scriptValueToIDBKey(&requestState, lowerValue);
    RefPtr<IDBKey> upper = scriptValueToIDBKey(&requestState, upperValue);

    if (!lower || !lower->isValid() || !upper || !upper->isValid()) {
        es.throwDOMException(DataError, IDBDatabase::notValidKeyErrorMessage);
        return 0;
    }
    if (upper->isLessThan(lower.get())) {
        es.throwDOMException(DataError, "The lower key is greater than the upper key.");
        return 0;
    }
    if (upper->isEqual(lower.get()) && (lowerOpen || upperOpen)) {
        es.throwDOMException(DataError, "The lower key and upper key are equal and one of the bounds is open.");
        return 0;
    }

    return IDBKeyRange::create(lower, upper, lowerOpen ? LowerBoundOpen : LowerBoundClosed, upperOpen ? UpperBoundOpen : UpperBoundClosed);
}

bool IDBKeyRange::isOnlyKey() const
{
    if (m_lowerType != LowerBoundClosed || m_upperType != UpperBoundClosed)
        return false;

    ASSERT(m_lower);
    ASSERT(m_upper);
    return m_lower->isEqual(m_upper.get());
}

} // namespace WebCore
