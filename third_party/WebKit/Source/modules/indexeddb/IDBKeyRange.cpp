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

#include "modules/indexeddb/IDBKeyRange.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ToV8ForCore.h"
#include "bindings/modules/v8/ToV8ForModules.h"
#include "bindings/modules/v8/V8BindingForModules.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "modules/indexeddb/IDBDatabase.h"

namespace blink {

IDBKeyRange* IDBKeyRange::FromScriptValue(ExecutionContext* context,
                                          const ScriptValue& value,
                                          ExceptionState& exception_state) {
  if (value.IsUndefined() || value.IsNull())
    return nullptr;

  IDBKeyRange* range =
      ScriptValue::To<IDBKeyRange*>(ToIsolate(context), value, exception_state);
  if (range)
    return range;

  IDBKey* key =
      ScriptValue::To<IDBKey*>(ToIsolate(context), value, exception_state);
  if (exception_state.HadException())
    return nullptr;
  if (!key || !key->IsValid()) {
    exception_state.ThrowDOMException(kDataError,
                                      IDBDatabase::kNotValidKeyErrorMessage);
    return nullptr;
  }

  return new IDBKeyRange(key, key, kLowerBoundClosed, kUpperBoundClosed);
}

IDBKeyRange::IDBKeyRange(IDBKey* lower,
                         IDBKey* upper,
                         LowerBoundType lower_type,
                         UpperBoundType upper_type)
    : lower_(lower),
      upper_(upper),
      lower_type_(lower_type),
      upper_type_(upper_type) {}

void IDBKeyRange::Trace(blink::Visitor* visitor) {
  visitor->Trace(lower_);
  visitor->Trace(upper_);
}

ScriptValue IDBKeyRange::lowerValue(ScriptState* script_state) const {
  return ScriptValue::From(script_state, lower_);
}

ScriptValue IDBKeyRange::upperValue(ScriptState* script_state) const {
  return ScriptValue::From(script_state, upper_);
}

IDBKeyRange* IDBKeyRange::only(IDBKey* key, ExceptionState& exception_state) {
  if (!key || !key->IsValid()) {
    exception_state.ThrowDOMException(kDataError,
                                      IDBDatabase::kNotValidKeyErrorMessage);
    return nullptr;
  }

  return IDBKeyRange::Create(key, key, kLowerBoundClosed, kUpperBoundClosed);
}

IDBKeyRange* IDBKeyRange::only(ScriptState* script_state,
                               const ScriptValue& key_value,
                               ExceptionState& exception_state) {
  IDBKey* key =
      ScriptValue::To<IDBKey*>(ToIsolate(ExecutionContext::From(script_state)),
                               key_value, exception_state);
  if (exception_state.HadException())
    return nullptr;
  if (!key || !key->IsValid()) {
    exception_state.ThrowDOMException(kDataError,
                                      IDBDatabase::kNotValidKeyErrorMessage);
    return nullptr;
  }

  return IDBKeyRange::Create(key, key, kLowerBoundClosed, kUpperBoundClosed);
}

IDBKeyRange* IDBKeyRange::lowerBound(ScriptState* script_state,
                                     const ScriptValue& bound_value,
                                     bool open,
                                     ExceptionState& exception_state) {
  IDBKey* bound =
      ScriptValue::To<IDBKey*>(ToIsolate(ExecutionContext::From(script_state)),
                               bound_value, exception_state);
  if (exception_state.HadException())
    return nullptr;
  if (!bound || !bound->IsValid()) {
    exception_state.ThrowDOMException(kDataError,
                                      IDBDatabase::kNotValidKeyErrorMessage);
    return nullptr;
  }

  return IDBKeyRange::Create(bound, nullptr,
                             open ? kLowerBoundOpen : kLowerBoundClosed,
                             kUpperBoundOpen);
}

IDBKeyRange* IDBKeyRange::upperBound(ScriptState* script_state,
                                     const ScriptValue& bound_value,
                                     bool open,
                                     ExceptionState& exception_state) {
  IDBKey* bound =
      ScriptValue::To<IDBKey*>(ToIsolate(ExecutionContext::From(script_state)),
                               bound_value, exception_state);
  if (exception_state.HadException())
    return nullptr;
  if (!bound || !bound->IsValid()) {
    exception_state.ThrowDOMException(kDataError,
                                      IDBDatabase::kNotValidKeyErrorMessage);
    return nullptr;
  }

  return IDBKeyRange::Create(nullptr, bound, kLowerBoundOpen,
                             open ? kUpperBoundOpen : kUpperBoundClosed);
}

IDBKeyRange* IDBKeyRange::bound(ScriptState* script_state,
                                const ScriptValue& lower_value,
                                const ScriptValue& upper_value,
                                bool lower_open,
                                bool upper_open,
                                ExceptionState& exception_state) {
  IDBKey* lower =
      ScriptValue::To<IDBKey*>(ToIsolate(ExecutionContext::From(script_state)),
                               lower_value, exception_state);
  if (exception_state.HadException())
    return nullptr;
  if (!lower || !lower->IsValid()) {
    exception_state.ThrowDOMException(kDataError,
                                      IDBDatabase::kNotValidKeyErrorMessage);
    return nullptr;
  }

  IDBKey* upper =
      ScriptValue::To<IDBKey*>(ToIsolate(ExecutionContext::From(script_state)),
                               upper_value, exception_state);
  if (exception_state.HadException())
    return nullptr;
  if (!upper || !upper->IsValid()) {
    exception_state.ThrowDOMException(kDataError,
                                      IDBDatabase::kNotValidKeyErrorMessage);
    return nullptr;
  }

  if (upper->IsLessThan(lower)) {
    exception_state.ThrowDOMException(
        kDataError, "The lower key is greater than the upper key.");
    return nullptr;
  }
  if (upper->IsEqual(lower) && (lower_open || upper_open)) {
    exception_state.ThrowDOMException(
        kDataError,
        "The lower key and upper key are equal and one of the bounds is open.");
    return nullptr;
  }

  return IDBKeyRange::Create(lower, upper,
                             lower_open ? kLowerBoundOpen : kLowerBoundClosed,
                             upper_open ? kUpperBoundOpen : kUpperBoundClosed);
}

bool IDBKeyRange::includes(ScriptState* script_state,
                           const ScriptValue& key_value,
                           ExceptionState& exception_state) {
  IDBKey* key =
      ScriptValue::To<IDBKey*>(ToIsolate(ExecutionContext::From(script_state)),
                               key_value, exception_state);
  if (exception_state.HadException())
    return false;
  if (!key || !key->IsValid()) {
    exception_state.ThrowDOMException(kDataError,
                                      IDBDatabase::kNotValidKeyErrorMessage);
    return false;
  }

  if (lower_) {
    short c = key->Compare(lower_);
    if (lowerOpen()) {
      if (c <= 0)
        return false;
    } else {
      if (c < 0)
        return false;
    }
  }

  if (upper_) {
    short c = key->Compare(upper_);
    if (upperOpen()) {
      if (c >= 0)
        return false;
    } else {
      if (c > 0)
        return false;
    }
  }
  return true;
}

}  // namespace blink
